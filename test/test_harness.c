/*
 * Standalone smoke test for FNVSpeedhack.dll (no game required).
 *
 * Loads the DLL like NVSE would, calls NVSEPlugin(), then verifies the
 * speed factor with un-hooked clocks (GetSystemTimeAsFileTime) against
 * hooked ones (QueryPerformanceCounter / GetTickCount):
 *
 *   speed 1.0 -> ratio ~ 1.0
 *   fnvspeed.txt "3.0" -> ratio ~ 3.0   (file IPC + status write-back)
 *   fnvspeed.txt "1.0" -> ratio ~ 1.0
 *
 * Expects the layout:  <dir>\Data\NVSE\Plugins\FNVSpeedhack.dll
 * Usage: test_harness.exe <dir>
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef BOOL (__cdecl *NVSEPlugin_t)(void*);

/* SpeedKeys bound in the test ini: F21=1x F22=3x F23=5x F24=10x */
#define TEST_VK_1X   0x84
#define TEST_VK_3X   0x85
#define TEST_VK_5X   0x86
#define TEST_VK_10X  0x87

static BOOL g_failed = FALSE;

static void check(const char* name, double value, double expected, double tol)
{
    BOOL ok = (value >= expected - tol) && (value <= expected + tol);
    printf("  %-34s got %7.3f expected %5.2f +- %.2f  %s\n",
           name, value, expected, tol, ok ? "OK" : "FAIL");
    if (!ok)
        g_failed = TRUE;
}

static ULONGLONG real_time_100ns(void)
{
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft); /* not hooked */
    return ((ULONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
}

/* ratio of hooked-QPC progress to real progress over `ms` milliseconds */
static double measure_qpc_ratio(DWORD ms)
{
    LARGE_INTEGER q0, q1, qpf;
    ULONGLONG r0, r1;
    double realSec, fakeSec;

    QueryPerformanceFrequency(&qpf);
    QueryPerformanceCounter(&q0);
    r0 = real_time_100ns();
    Sleep(ms);
    QueryPerformanceCounter(&q1);
    r1 = real_time_100ns();

    realSec = (double)(r1 - r0) / 10000000.0;
    fakeSec = (double)(q1.QuadPart - q0.QuadPart) / (double)qpf.QuadPart;
    return fakeSec / realSec;
}

static double measure_tick_ratio(DWORD ms)
{
    DWORD t0, t1;
    ULONGLONG r0, r1;
    double realSec, fakeSec;

    t0 = GetTickCount();
    r0 = real_time_100ns();
    Sleep(ms);
    t1 = GetTickCount();
    r1 = real_time_100ns();

    realSec = (double)(r1 - r0) / 10000000.0;
    fakeSec = (double)(t1 - t0) / 1000.0;
    return fakeSec / realSec;
}

/* inject a key press, hold it past the hotkey poll interval, then release */
static void send_key(BYTE vk)
{
    INPUT in;
    memset(&in, 0, sizeof(in));
    in.type = INPUT_KEYBOARD;

    in.ki.wVk = vk;
    if (!SendInput(1, &in, sizeof(INPUT))) {
        printf("  SendInput failed (err=%lu)\n", GetLastError());
        g_failed = TRUE;
        return;
    }
    Sleep(100); /* hold > HotkeyPollMs (30 ms) so the poller sees the edge */

    in.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &in, sizeof(INPUT));
}

static void write_control(const char* root, const char* text)
{
    char path[MAX_PATH];
    FILE* f;
    _snprintf(path, sizeof(path), "%s\\fnvspeed.txt", root);
    f = fopen(path, "w");
    if (!f) {
        printf("  cannot write %s\n", path);
        g_failed = TRUE;
        return;
    }
    fputs(text, f);
    fclose(f);
}

static BOOL read_status(const char* root, double* out)
{
    char path[MAX_PATH];
    FILE* f;
    _snprintf(path, sizeof(path), "%s\\fnvspeed_status.txt", root);
    f = fopen(path, "r");
    if (!f)
        return FALSE;
    fscanf(f, "%lf", out);
    fclose(f);
    return TRUE;
}

int main(int argc, char** argv)
{
    char dir[MAX_PATH];
    char dllPath[MAX_PATH];
    HMODULE dll;
    NVSEPlugin_t plugin;
    double status;

    if (argc < 2) {
        printf("usage: test_harness.exe <dir containing Data\\NVSE\\Plugins\\FNVSpeedhack.dll>\n");
        return 2;
    }
    strncpy(dir, argv[1], MAX_PATH - 1);
    dir[MAX_PATH - 1] = 0;
    _snprintf(dllPath, sizeof(dllPath), "%s\\Data\\NVSE\\Plugins\\FNVSpeedhack.dll", dir);

    printf("[*] loading %s\n", dllPath);
    dll = LoadLibraryA(dllPath);
    if (!dll) {
        printf("[!] LoadLibrary failed, err=%lu\n", GetLastError());
        return 1;
    }

    plugin = (NVSEPlugin_t)GetProcAddress(dll, "NVSEPlugin");
    if (!plugin || !plugin(NULL)) {
        printf("[!] NVSEPlugin init failed\n");
        return 1;
    }
    printf("[*] plugin initialised\n");

    Sleep(300);
    printf("[*] measuring at default speed...\n");
    check("QPC ratio @ 1.0",  measure_qpc_ratio(1000), 1.0, 0.15);
    check("GetTickCount ratio @ 1.0", measure_tick_ratio(1000), 1.0, 0.15);

    printf("[*] writing fnvspeed.txt = 3.0\n");
    write_control(dir, "3.0\n");
    Sleep(700); /* file poller tick is 250 ms */
    if (!read_status(dir, &status)) {
        printf("  status file missing FAIL\n");
        g_failed = TRUE;
    } else {
        check("status write-back", status, 3.0, 0.01);
    }
    check("QPC ratio @ 3.0", measure_qpc_ratio(1000), 3.0, 0.3);
    check("GetTickCount ratio @ 3.0", measure_tick_ratio(1000), 3.0, 0.3);

    printf("[*] writing fnvspeed.txt = 0.5\n");
    write_control(dir, "0.5\n");
    Sleep(700);
    check("QPC ratio @ 0.5", measure_qpc_ratio(1500), 0.5, 0.15);

    printf("[*] writing fnvspeed.txt = reset\n");
    write_control(dir, "reset\n");
    Sleep(700);
    check("QPC ratio after reset", measure_qpc_ratio(1000), 1.0, 0.15);

    /* hotkey tests: inject F21..F24 like Steam Input paddles would */
    if (argc >= 3 && strcmp(argv[2], "noinput") == 0) {
        printf("[*] skipping hotkey injection tests (noinput)\n");
    } else {
        printf("[*] testing SpeedKeys hotkeys (injected F21..F24)\n");

        send_key(TEST_VK_10X);
        Sleep(500);
        if (read_status(dir, &status))
            check("status after 10x key", status, 10.0, 0.01);
        check("QPC ratio after 10x key", measure_qpc_ratio(700), 10.0, 1.0);

        send_key(TEST_VK_5X);
        Sleep(500);
        check("QPC ratio after 5x key", measure_qpc_ratio(700), 5.0, 0.5);

        send_key(TEST_VK_3X);
        Sleep(500);
        check("QPC ratio after 3x key", measure_qpc_ratio(700), 3.0, 0.3);

        send_key(TEST_VK_1X);
        Sleep(500);
        check("QPC ratio after 1x key", measure_qpc_ratio(700), 1.0, 0.15);
    }

    if (g_failed) {
        printf("[FAIL] test failed\n");
        return 1;
    }
    printf("[OK] all checks passed\n");
    return 0;
}
