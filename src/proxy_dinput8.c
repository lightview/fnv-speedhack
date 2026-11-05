/*
 * FNVSpeedhack - dinput8 proxy build.
 *
 * FalloutNV.exe imports DINPUT8.dll, so a proxy with this name in the game
 * folder is loaded by the exe itself - on Windows automatically, and under
 * Wine/Proton with WINEDLLOVERRIDES="dinput8=n,b". This makes the speedhack
 * independent from NVSE and from the game exe version.
 *
 * All six dinput8 exports are forwarded to the real system dinput8; the
 * speedhack hooks are installed on the first forwarded call (DirectInput8Create
 * is always the game's first dinput8 call, and by then the loader lock is
 * long gone).
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include "config.h"
#include "hooks.h"
#include "control.h"
#include "log.h"

#define FNVSPD_PROXY_VERSION "0.3.0-proxy"

typedef HRESULT(WINAPI *DI8CREATE)(HINSTANCE, DWORD, const void*, void**, void*);
typedef HRESULT(WINAPI *DI0VOID)(void);
typedef HRESULT(WINAPI *DI0CLS)(const void*, const void*, void**);
typedef const void* (WINAPI *DI0DF)(void);

static DI8CREATE p_DirectInput8Create;
static DI0VOID   p_DllCanUnloadNow;
static DI0CLS    p_DllGetClassObject;
static DI0VOID   p_DllRegisterServer;
static DI0VOID   p_DllUnregisterServer;
static DI0DF     p_GetdfDIJoystick;

static HMODULE g_hSelf = NULL;
static BOOL    g_realResolved = FALSE;
static BOOL    g_inited = FALSE;

static void get_dll_dir(char* out, size_t size)
{
    char path[MAX_PATH];
    char* slash;

    if (!GetModuleFileNameA(g_hSelf, path, MAX_PATH))
        path[0] = 0;
    slash = strrchr(path, '\\');
    if (slash)
        *slash = 0;
    lstrcpynA(out, path, (int)size);
}

static BOOL resolve_real(void)
{
    char path[MAX_PATH];
    UINT n;
    HMODULE real;

    if (g_realResolved)
        return (p_DirectInput8Create != NULL);
    g_realResolved = TRUE;

    /* full path so we cannot load ourselves again */
    n = GetSystemDirectoryA(path, MAX_PATH);
    if (!n || n + 12 >= MAX_PATH)
        return FALSE;
    lstrcatA(path, "\\dinput8.dll");

    real = LoadLibraryA(path);
    if (!real) {
        log_write("cannot load real dinput8 from %s (err %u)", path, GetLastError());
        return FALSE;
    }

    /* DirectInput8Create is the only export the game truly needs.
     * The rest resolve independently: Wine's builtin dinput8 has no
     * GetdfDIJoystick, and its absence must not break input. */
    p_DirectInput8Create =
        (DI8CREATE)(FARPROC)GetProcAddress(real, "DirectInput8Create");
    if (!p_DirectInput8Create) {
        log_write("missing export DirectInput8Create");
        return FALSE;
    }
    p_DllCanUnloadNow     = (DI0VOID)(FARPROC)GetProcAddress(real, "DllCanUnloadNow");
    p_DllGetClassObject   = (DI0CLS)(FARPROC)GetProcAddress(real, "DllGetClassObject");
    p_DllRegisterServer   = (DI0VOID)(FARPROC)GetProcAddress(real, "DllRegisterServer");
    p_DllUnregisterServer = (DI0VOID)(FARPROC)GetProcAddress(real, "DllUnregisterServer");
    p_GetdfDIJoystick     = (DI0DF)(FARPROC)GetProcAddress(real, "GetdfDIJoystick");
    if (!p_GetdfDIJoystick)
        log_write("GetdfDIJoystick unavailable in real dinput8 (ok)");
    return TRUE;
}

static void init_speedhack(void)
{
    char dllDir[MAX_PATH];
    Config cfg;

    if (g_inited)
        return;
    g_inited = TRUE; /* never retry: forwarding must keep working */

    get_dll_dir(dllDir, sizeof(dllDir));

    cfg_load(dllDir, &cfg);
    log_init(dllDir, cfg.logEnabled);
    log_write("FNVSpeedhack %s (dinput8 proxy)", FNVSPD_PROXY_VERSION);
    log_write("proxy dir: %s", dllDir);

    if (!hooks_install()) {
        log_write("hooks failed - forwarding continues without speed control");
        return;
    }
    log_write("timing hooks installed");

    speed_set_presets(cfg.presets, cfg.presetCount, cfg.startupSpeed);

    if (!control_start(g_hSelf, &cfg)) {
        log_write("control threads failed");
        return;
    }
    log_write("ready: presets=%d startup=%.2f", cfg.presetCount, cfg.startupSpeed);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    (void)lpvReserved;
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        g_hSelf = hinstDLL;
    } else if (fdwReason == DLL_PROCESS_DETACH && lpvReserved == NULL) {
        control_stop();
        hooks_shutdown();
        log_shutdown();
    }
    return TRUE;
}

/* ---- forwarded exports ---- */

HRESULT WINAPI DirectInput8Create(HINSTANCE hinst, DWORD dwVersion,
                                  const void* riid, void** ppvOut, void* punkOuter)
{
    init_speedhack();
    if (resolve_real() && p_DirectInput8Create)
        return p_DirectInput8Create(hinst, dwVersion, riid, ppvOut, punkOuter);
    if (ppvOut)
        *ppvOut = NULL;
    return 0x800401F0L; /* CO_E_NOTINITIALIZED, harmless here */
}

HRESULT WINAPI DllCanUnloadNow(void)
{
    init_speedhack();
    return p_DllCanUnloadNow ? p_DllCanUnloadNow() : 0x80040000L;
}

HRESULT WINAPI DllGetClassObject(const void* rclsid, const void* riid, void** ppv)
{
    init_speedhack();
    if (p_DllGetClassObject)
        return p_DllGetClassObject(rclsid, riid, ppv);
    if (ppv)
        *ppv = NULL;
    return 0x80004001L; /* E_NOTIMPL */
}

HRESULT WINAPI DllRegisterServer(void)
{
    return p_DllRegisterServer ? p_DllRegisterServer() : 0x80004005L;
}

HRESULT WINAPI DllUnregisterServer(void)
{
    return p_DllUnregisterServer ? p_DllUnregisterServer() : 0x80004005L;
}

const void* WINAPI GetdfDIJoystick(void)
{
    init_speedhack();
    return p_GetdfDIJoystick ? p_GetdfDIJoystick() : NULL;
}
