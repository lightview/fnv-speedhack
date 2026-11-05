/*
 * FNVSpeedhack - Cheat Engine style game speed control.
 *
 * Hooks the Windows timing APIs and returns scaled values, exactly like
 * Cheat Engine's speedhack.dll does:
 *   - QueryPerformanceCounter (kernel32)  <- Gamebryo engine clock
 *   - GetTickCount / GetTickCount64 (kernel32)
 *   - timeGetTime (winmm)
 *
 * Each source keeps its own (lastReal, lastFake) pair. On every call the
 * real delta is multiplied by the speed factor (with a fractional carry)
 * and accumulated into the fake value. Changing the speed factor between
 * calls is therefore seamless - no time jumps in either direction.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <MinHook.h>
#include "hooks.h"
#include "log.h"

/* ---- trampolines to the original functions ---- */
static BOOL      (WINAPI *pQueryPerformanceCounter)(LARGE_INTEGER*) = NULL;
static DWORD     (WINAPI *pGetTickCount)(void) = NULL;
static ULONGLONG (WINAPI *pGetTickCount64)(void) = NULL;
static DWORD     (WINAPI *pTimeGetTime)(void) = NULL;

/* ---- shared state ---- */
static CRITICAL_SECTION g_cs;
static double g_speed = 1.0;
static int    g_installed = 0;

/* QueryPerformanceCounter state */
static LONGLONG      g_qpcFake;
static LARGE_INTEGER g_qpcReal;
static double        g_qpcCarry;
static int           g_qpcValid;

/* GetTickCount / GetTickCount64 share one epoch (ms since boot) */
static ULONGLONG g_tickFake;
static DWORD     g_tickReal;
static double    g_tickCarry;
static int       g_tickValid;

/* timeGetTime kept separate: its epoch origin can differ under Wine */
static ULONGLONG g_gttFake;
static DWORD     g_gttReal;
static double    g_gttCarry;
static int       g_gttValid;

/* ---- presets ---- */
static float g_presets[FNVSPD_MAX_PRESETS] = { 1.0f, 2.0f, 3.0f, 5.0f, 10.0f };
static int   g_presetCount = 5;
static int   g_presetIdx = 0;

#define FNVSPD_MIN_SPEED 0.05
#define FNVSPD_MAX_SPEED 50.0

static double clamp_speed(double v)
{
    if (v < FNVSPD_MIN_SPEED) v = FNVSPD_MIN_SPEED;
    if (v > FNVSPD_MAX_SPEED) v = FNVSPD_MAX_SPEED;
    return v;
}

static int match_preset(double v)
{
    int i;
    for (i = 0; i < g_presetCount; i++) {
        if ((double)g_presets[i] == v)
            return i;
    }
    return -1;
}

/* ---- hooked functions ---- */

static BOOL WINAPI Hooked_QPC(LARGE_INTEGER* lpCount)
{
    LARGE_INTEGER real;
    BOOL ok;

    if (!lpCount)
        return pQueryPerformanceCounter(lpCount);

    ok = pQueryPerformanceCounter(&real);
    if (!ok)
        return ok;

    EnterCriticalSection(&g_cs);
    if (!g_qpcValid) {
        g_qpcReal  = real;
        g_qpcFake  = real.QuadPart;
        g_qpcCarry = 0.0;
        g_qpcValid = 1;
    } else {
        double delta = (double)(real.QuadPart - g_qpcReal.QuadPart);
        g_qpcCarry += delta * g_speed;
        g_qpcFake  += (LONGLONG)g_qpcCarry;
        g_qpcCarry -= (double)(LONGLONG)g_qpcCarry;
        g_qpcReal   = real;
    }
    lpCount->QuadPart = g_qpcFake;
    LeaveCriticalSection(&g_cs);
    return TRUE;
}

static DWORD WINAPI Hooked_GetTickCount(void)
{
    DWORD real = pGetTickCount();

    EnterCriticalSection(&g_cs);
    if (!g_tickValid) {
        g_tickReal  = real;
        g_tickFake  = real;
        g_tickCarry = 0.0;
        g_tickValid = 1;
    } else {
        g_tickCarry += (double)(DWORD)(real - g_tickReal) * g_speed;
        g_tickFake  += (ULONGLONG)g_tickCarry;
        g_tickCarry -= (double)(ULONGLONG)g_tickCarry;
        g_tickReal   = real;
    }
    LeaveCriticalSection(&g_cs);
    return (DWORD)g_tickFake;
}

static ULONGLONG WINAPI Hooked_GetTickCount64(void)
{
    ULONGLONG real = pGetTickCount64();

    EnterCriticalSection(&g_cs);
    if (!g_tickValid) {
        g_tickReal  = (DWORD)real;
        g_tickFake  = real;
        g_tickCarry = 0.0;
        g_tickValid = 1;
    } else {
        g_tickCarry += (double)(DWORD)(real - (ULONGLONG)g_tickReal) * g_speed;
        g_tickFake  += (ULONGLONG)g_tickCarry;
        g_tickCarry -= (double)(ULONGLONG)g_tickCarry;
        g_tickReal   = (DWORD)real;
    }
    LeaveCriticalSection(&g_cs);
    return g_tickFake;
}

static DWORD WINAPI Hooked_timeGetTime(void)
{
    DWORD real = pTimeGetTime();

    EnterCriticalSection(&g_cs);
    if (!g_gttValid) {
        g_gttReal  = real;
        g_gttFake  = real;
        g_gttCarry = 0.0;
        g_gttValid = 1;
    } else {
        g_gttCarry += (double)(DWORD)(real - g_gttReal) * g_speed;
        g_gttFake  += (ULONGLONG)g_gttCarry;
        g_gttCarry -= (double)(ULONGLONG)g_gttCarry;
        g_gttReal   = real;
    }
    LeaveCriticalSection(&g_cs);
    return (DWORD)g_gttFake;
}

/* ---- install / shutdown ---- */

/* One live speedhack per process: the NVSE plugin and the dinput8 proxy may
 * both be present (MO2 setups), the second one to load must stay passive. */
static HANDLE g_singleton = NULL;

BOOL hooks_install(void)
{
    HMODULE winmm;

    g_singleton = CreateMutexA(NULL, TRUE, "FNVSpeedhack_SingleInstance");
    if (g_singleton && GetLastError() == ERROR_ALREADY_EXISTS) {
        log_write("another FNVSpeedhack instance is active - staying passive");
        return FALSE;
    }

    InitializeCriticalSection(&g_cs);

    /* winmm is normally already loaded by the game; make sure anyway */
    winmm = GetModuleHandleA("winmm.dll");
    if (!winmm)
        winmm = LoadLibraryA("winmm.dll");

    if (MH_Initialize() != MH_OK) {
        log_write("MH_Initialize failed");
        return FALSE;
    }

    /* the three timers the engine relies on must hook, or we bail out */
    if (MH_CreateHookApi(L"kernel32", "QueryPerformanceCounter",
                         Hooked_QPC, (LPVOID*)&pQueryPerformanceCounter) != MH_OK) {
        log_write("hook QueryPerformanceCounter failed");
        return FALSE;
    }
    if (MH_CreateHookApi(L"kernel32", "GetTickCount",
                         Hooked_GetTickCount, (LPVOID*)&pGetTickCount) != MH_OK) {
        log_write("hook GetTickCount failed");
        return FALSE;
    }
    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        log_write("MH_EnableHook failed");
        return FALSE;
    }

    /* optional timers: keep going even if these are unavailable */
    if (MH_CreateHookApi(L"kernel32", "GetTickCount64",
                         Hooked_GetTickCount64, (LPVOID*)&pGetTickCount64) == MH_OK) {
        if (MH_EnableHook(Hooked_GetTickCount64) != MH_OK)
            pGetTickCount64 = NULL;
    } else {
        log_write("hook GetTickCount64 unavailable (ok)");
    }
    if (winmm &&
        MH_CreateHookApi(L"winmm", "timeGetTime",
                         Hooked_timeGetTime, (LPVOID*)&pTimeGetTime) == MH_OK) {
        if (MH_EnableHook(Hooked_timeGetTime) != MH_OK)
            pTimeGetTime = NULL;
    } else {
        log_write("hook timeGetTime unavailable (ok)");
    }

    g_installed = 1;
    return TRUE;
}

void hooks_shutdown(void)
{
    if (!g_installed)
        return;
    g_installed = 0;
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    DeleteCriticalSection(&g_cs);
    if (g_singleton) {
        CloseHandle(g_singleton);
        g_singleton = NULL;
    }
}

DWORD hooks_real_time_ms(void)
{
    if (pTimeGetTime)
        return pTimeGetTime();
    if (pGetTickCount)
        return pGetTickCount();
    return GetTickCount();
}

/* ---- speed engine ---- */

void speed_set_presets(const float* presets, int count, float startupSpeed)
{
    int i;

    if (count > FNVSPD_MAX_PRESETS)
        count = FNVSPD_MAX_PRESETS;
    if (count <= 0) {
        g_presets[0] = 2.0f;
        g_presetCount = 1;
    } else {
        for (i = 0; i < count; i++)
            g_presets[i] = presets[i];
        g_presetCount = count;
    }

    speed_set((double)startupSpeed);
}

double speed_set(double v)
{
    v = clamp_speed(v);
    EnterCriticalSection(&g_cs);
    g_speed = v;
    g_presetIdx = match_preset(v);
    LeaveCriticalSection(&g_cs);
    return v;
}

double speed_cycle(void)
{
    double v;

    EnterCriticalSection(&g_cs);
    g_presetIdx = (g_presetIdx < 0) ? 0 : (g_presetIdx + 1) % g_presetCount;
    v = g_presets[g_presetIdx];
    g_speed = v;
    LeaveCriticalSection(&g_cs);
    return (double)v;
}

double speed_step(int dir)
{
    double v;

    EnterCriticalSection(&g_cs);
    if (g_presetIdx < 0)
        g_presetIdx = 0;
    else {
        g_presetIdx += dir;
        if (g_presetIdx < 0) g_presetIdx = 0;
        if (g_presetIdx >= g_presetCount) g_presetIdx = g_presetCount - 1;
    }
    v = g_presets[g_presetIdx];
    g_speed = v;
    LeaveCriticalSection(&g_cs);
    return (double)v;
}

double speed_get(void)
{
    double v;
    EnterCriticalSection(&g_cs);
    v = g_speed;
    LeaveCriticalSection(&g_cs);
    return v;
}

int speed_preset_index(void)
{
    int idx;
    EnterCriticalSection(&g_cs);
    idx = g_presetIdx;
    LeaveCriticalSection(&g_cs);
    return idx;
}

float* speed_presets(int* count)
{
    if (count)
        *count = g_presetCount;
    return g_presets;
}
