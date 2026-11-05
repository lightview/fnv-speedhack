/*
 * Control plane: hotkeys (GetAsyncKeyState poller) and file IPC.
 *
 * File IPC (used by the Steam Deck Decky plugin, but works everywhere):
 *   <game root>\fnvspeed.txt         - command file: "3.0" / "toggle" / "up" / "down" / "reset"
 *   <game root>\fnvspeed_status.txt  - current speed, written back by the DLL
 * The command file is deleted after processing, so "toggle" style commands
 * fire exactly once.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include "control.h"
#include "hooks.h"
#include "log.h"

static HANDLE g_stopEvent = NULL;
static HANDLE g_hkThread  = NULL;
static HANDLE g_fileThread = NULL;

static Config g_cfg;
static char g_dllDir[MAX_PATH];
static char g_gameRoot[MAX_PATH];
static char g_ctlPath[MAX_PATH];
static char g_statusPath[MAX_PATH];

/* ---- status write-back ---- */

static void write_status(void)
{
    char tmpPath[MAX_PATH];
    FILE* f;

    if (!g_cfg.controlFileEnabled)
        return;

    _snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", g_statusPath);
    tmpPath[sizeof(tmpPath) - 1] = 0;

    f = fopen(tmpPath, "w");
    if (!f)
        return;
    fprintf(f, "%.2f\n", speed_get());
    fclose(f);
    MoveFileExA(tmpPath, g_statusPath, MOVEFILE_REPLACE_EXISTING);
}

static void apply_change(double v, const char* source)
{
    int idx = speed_preset_index();

    log_write("speed -> %.2f (%s)", v, source);
    write_status();

    if (g_cfg.beepFeedback) {
        /* one tone per preset; a higher tone for custom values */
        Beep(idx >= 0 ? 700 + 300 * idx : 1600, 70);
    }
}

/* ---- hotkey thread ---- */

static int key_edge(WORD vk, BYTE* wasDown)
{
    SHORT s;
    int down;

    if (!vk)
        return 0;
    s = GetAsyncKeyState((int)vk);
    down = (s & 0x8000) ? 1 : 0;
    if (down && !*wasDown) {
        *wasDown = 1;
        return 1;
    }
    if (!down)
        *wasDown = 0;
    return 0;
}

static unsigned __stdcall hotkey_thread(void* arg)
{
    BYTE prevCycle = 0, prevUp = 0, prevDown = 0;
    BYTE prevKeys[FNVSPD_MAX_SPEEDKEYS] = { 0 };
    int i;

    (void)arg;
    log_write("hotkey thread: cycle=%u up=%u down=%u",
              g_cfg.vkCycle, g_cfg.vkUp, g_cfg.vkDown);
    for (i = 0; i < g_cfg.speedKeyCount; i++)
        log_write("hotkey thread: vk %u -> %.1fx", g_cfg.speedKeys[i].vk,
                  g_cfg.speedKeys[i].speed);

    while (WaitForSingleObject(g_stopEvent, g_cfg.hotkeyPollMs) == WAIT_TIMEOUT) {
        if (key_edge(g_cfg.vkCycle, &prevCycle))
            apply_change(speed_cycle(), "hotkey cycle");
        if (key_edge(g_cfg.vkUp, &prevUp))
            apply_change(speed_step(1), "hotkey up");
        if (key_edge(g_cfg.vkDown, &prevDown))
            apply_change(speed_step(-1), "hotkey down");

        for (i = 0; i < g_cfg.speedKeyCount; i++) {
            if (key_edge(g_cfg.speedKeys[i].vk, &prevKeys[i]))
                apply_change(speed_set((double)g_cfg.speedKeys[i].speed),
                             "hotkey speed");
        }
    }
    return 0;
}

/* ---- control file thread ---- */

static void parse_and_apply(const char* buf)
{
    /* last non-empty line wins */
    const char* line = NULL;
    const char* p = buf;
    const char* eol;

    while (*p) {
        eol = strchr(p, '\n');
        if (!eol)
            eol = p + strlen(p);
        if (eol > p) {
            const char* q = p;
            while (q < eol && (*q == ' ' || *q == '\t' || *q == '\r'))
                q++;
            if (q < eol)
                line = q;
        }
        if (!*eol)
            break;
        p = eol + 1;
    }

    if (!line)
        return;

    if (!_strnicmp(line, "toggle", 6) || !_strnicmp(line, "cycle", 5)) {
        apply_change(speed_cycle(), "file toggle");
    } else if (!_strnicmp(line, "up", 2)) {
        apply_change(speed_step(1), "file up");
    } else if (!_strnicmp(line, "down", 4)) {
        apply_change(speed_step(-1), "file down");
    } else if (!_strnicmp(line, "reset", 5)) {
        apply_change(speed_set((double)g_cfg.startupSpeed), "file reset");
    } else {
        char* end;
        double v = strtod(line, &end);
        if (end != line && v > 0.0)
            apply_change(speed_set(v), "file value");
        else
            log_write("control file: unrecognised '%.32s'", line);
    }
}

static unsigned __stdcall file_thread(void* arg)
{
    (void)arg;
    log_write("file thread: %s", g_ctlPath);

    write_status(); /* publish initial speed for the Decky plugin */

    while (WaitForSingleObject(g_stopEvent, g_cfg.controlPollMs) == WAIT_TIMEOUT) {
        HANDLE h;
        char buf[256];
        DWORD rd = 0;

        if (!g_cfg.controlFileEnabled)
            continue;

        h = CreateFileA(g_ctlPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                        NULL, OPEN_EXISTING, 0, NULL);
        if (h == INVALID_HANDLE_VALUE)
            continue;

        memset(buf, 0, sizeof(buf));
        ReadFile(h, buf, sizeof(buf) - 1, &rd, NULL);
        CloseHandle(h);

        if (rd > 0) {
            parse_and_apply(buf);
            DeleteFileA(g_ctlPath); /* consume the command */
        }
    }
    return 0;
}

/* ---- lifecycle ---- */

static void join_and_close(HANDLE* t)
{
    if (*t) {
        WaitForSingleObject(*t, 2000);
        CloseHandle(*t);
        *t = NULL;
    }
}

BOOL control_start(HMODULE hSelf, const Config* cfg)
{
    char path[MAX_PATH];
    char* slash;

    (void)hSelf;
    g_cfg = *cfg;

    GetModuleFileNameA(hSelf, path, MAX_PATH);
    slash = strrchr(path, '\\');
    if (slash)
        *slash = 0;
    strncpy(g_dllDir, path, MAX_PATH - 1);
    g_dllDir[MAX_PATH - 1] = 0;

    /* game root: Data\NVSE\Plugins -> ..\..\.. */
    _snprintf(path, sizeof(path), "%s\\..\\..\\..", g_dllDir);
    path[sizeof(path) - 1] = 0;
    if (!GetFullPathNameA(path, MAX_PATH, g_gameRoot, NULL)) {
        log_write("cannot resolve game root");
        return FALSE;
    }

    _snprintf(g_ctlPath,    sizeof(g_ctlPath),    "%s\\fnvspeed.txt", g_gameRoot);
    _snprintf(g_statusPath, sizeof(g_statusPath), "%s\\fnvspeed_status.txt", g_gameRoot);
    g_ctlPath[MAX_PATH - 1] = 0;
    g_statusPath[MAX_PATH - 1] = 0;

    log_write("game root: %s", g_gameRoot);

    g_stopEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!g_stopEvent)
        return FALSE;

    g_hkThread = (HANDLE)_beginthreadex(NULL, 0, hotkey_thread, NULL, 0, NULL);
    g_fileThread = (HANDLE)_beginthreadex(NULL, 0, file_thread, NULL, 0, NULL);

    if (!g_hkThread || !g_fileThread) {
        control_stop();
        return FALSE;
    }
    return TRUE;
}

void control_stop(void)
{
    if (g_stopEvent)
        SetEvent(g_stopEvent);
    join_and_close(&g_hkThread);
    join_and_close(&g_fileThread);
    if (g_stopEvent) {
        CloseHandle(g_stopEvent);
        g_stopEvent = NULL;
    }
}
