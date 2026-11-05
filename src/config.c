#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"

#define INI_SECTION "speedhack"

static WORD get_vk(const char* iniPath, const char* key, const char* def)
{
    char buf[64];
    GetPrivateProfileStringA(INI_SECTION, key, def, buf, sizeof(buf), iniPath);
    /* accepts decimal ("119") or hex with 0x prefix ("0x77") */
    return (WORD)(ULONG_PTR)strtoul(buf, NULL, 0);
}

static void parse_presets(const char* s, float* out, int* count)
{
    char* end;
    double v;

    *count = 0;
    while (*count < FNVSPD_MAX_PRESETS) {
        while (*s == ',' || *s == ';' || *s == ' ' || *s == '\t')
            s++;
        if (*s == 0)
            break;
        v = strtod(s, &end);
        if (end == s)
            break;
        if (v > 0.0)
            out[(*count)++] = (float)v;
        s = end;
    }
}

/* parses "vk:speed,vk:speed,..." entries, VK in decimal or 0x-hex */
static void parse_speedkeys(const char* s, SpeedKey* out, int* count)
{
    char* end;
    double v;
    unsigned long vk;

    *count = 0;
    while (*count < FNVSPD_MAX_SPEEDKEYS) {
        while (*s == ',' || *s == ';' || *s == ' ' || *s == '\t')
            s++;
        if (*s == 0)
            break;
        vk = strtoul(s, &end, 0);
        if (end == s)
            break;
        s = end;
        while (*s == ':' || *s == '=' || *s == ' ')
            s++;
        v = strtod(s, &end);
        if (end == s || v <= 0.0)
            break;
        s = end;
        out[*count].vk = (WORD)vk;
        out[*count].speed = (float)v;
        (*count)++;
    }
}

void cfg_load(const char* dllDir, Config* cfg)
{
    char iniPath[MAX_PATH];
    char buf[256];

    _snprintf(iniPath, sizeof(iniPath), "%s\\FNVSpeedhack.ini", dllDir);
    iniPath[sizeof(iniPath) - 1] = 0;

    memset(cfg, 0, sizeof(*cfg));

    cfg->vkCycle = get_vk(iniPath, "HotkeyCycle", "119"); /* F8  */
    cfg->vkUp    = get_vk(iniPath, "HotkeyUp",    "120"); /* F9  */
    cfg->vkDown  = get_vk(iniPath, "HotkeyDown",  "121"); /* F10 */

    /* direct binds: Numpad1..4 AND top-row 1..4 both map to 1x/3x/5x/10x,
     * covering either Steam Input keypad choice */
    GetPrivateProfileStringA(INI_SECTION, "SpeedKeys",
                             "97:1.0,98:3.0,99:5.0,100:10.0,"
                             "49:1.0,50:3.0,51:5.0,52:10.0",
                             buf, sizeof(buf), iniPath);
    parse_speedkeys(buf, cfg->speedKeys, &cfg->speedKeyCount);

    /* gamepad combos: LB+RB held, then A/B/X/Y - no Steam Input needed */
    GetPrivateProfileStringA(INI_SECTION, "PadModifier", "LB+RB",
                             cfg->padModifier, sizeof(cfg->padModifier), iniPath);
    GetPrivateProfileStringA(INI_SECTION, "PadA", "1.0", buf, sizeof(buf), iniPath);
    cfg->padA = (float)strtod(buf, NULL);
    GetPrivateProfileStringA(INI_SECTION, "PadB", "3.0", buf, sizeof(buf), iniPath);
    cfg->padB = (float)strtod(buf, NULL);
    GetPrivateProfileStringA(INI_SECTION, "PadX", "5.0", buf, sizeof(buf), iniPath);
    cfg->padX = (float)strtod(buf, NULL);
    GetPrivateProfileStringA(INI_SECTION, "PadY", "10.0", buf, sizeof(buf), iniPath);
    cfg->padY = (float)strtod(buf, NULL);

    GetPrivateProfileStringA(INI_SECTION, "Presets", "1.0,2.0,3.0,5.0,10.0",
                             buf, sizeof(buf), iniPath);
    parse_presets(buf, cfg->presets, &cfg->presetCount);
    if (cfg->presetCount == 0) {
        cfg->presets[0] = 1.0f; cfg->presets[1] = 2.0f;
        cfg->presets[2] = 3.0f; cfg->presets[3] = 5.0f;
        cfg->presets[4] = 10.0f;
        cfg->presetCount = 5;
    }

    GetPrivateProfileStringA(INI_SECTION, "StartupSpeed", "1.0",
                             buf, sizeof(buf), iniPath);
    cfg->startupSpeed = (float)strtod(buf, NULL);
    if (cfg->startupSpeed <= 0.0f)
        cfg->startupSpeed = 1.0f;

    cfg->controlFileEnabled = GetPrivateProfileIntA(INI_SECTION, "ControlFileEnabled", 1, iniPath);
    cfg->hotkeyPollMs       = (DWORD)GetPrivateProfileIntA(INI_SECTION, "HotkeyPollMs", 30, iniPath);
    cfg->controlPollMs      = (DWORD)GetPrivateProfileIntA(INI_SECTION, "ControlPollMs", 250, iniPath);
    cfg->beepFeedback       = GetPrivateProfileIntA(INI_SECTION, "BeepFeedback", 1, iniPath);
    cfg->logEnabled         = GetPrivateProfileIntA(INI_SECTION, "LogEnabled", 1, iniPath);

    if (cfg->hotkeyPollMs  < 10)    cfg->hotkeyPollMs  = 10;
    if (cfg->controlPollMs < 50)    cfg->controlPollMs = 50;
}
