/*
 * FNVSpeedhack - NVSE plugin entry point.
 *
 * NVSE loads this DLL from Data\NVSE\Plugins\ and calls the exported
 * NVSEPlugin(). We use no NVSE APIs, so the plugin is engine-version
 * independent and works the same on Windows and under Wine/Proton.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include "config.h"
#include "hooks.h"
#include "control.h"
#include "log.h"

#define FNVSPD_VERSION "0.2.0"

static HMODULE g_hSelf = NULL;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        g_hSelf = hinstDLL;
    } else if (fdwReason == DLL_PROCESS_DETACH && lpvReserved == NULL) {
        /* FreeLibrary path only; on process exit nothing is safe to do */
        control_stop();
        hooks_shutdown();
        log_shutdown();
    }
    return TRUE;
}

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

/* NVSE calls this after loading the DLL. The interface pointer is unused. */
BOOL __cdecl NVSEPlugin(void* nvseInterface)
{
    char dllDir[MAX_PATH];
    Config cfg;

    (void)nvseInterface;

    get_dll_dir(dllDir, sizeof(dllDir));

    cfg_load(dllDir, &cfg);
    log_init(dllDir, cfg.logEnabled);

    log_write("FNVSpeedhack v%s", FNVSPD_VERSION);
    log_write("plugin dir: %s", dllDir);

    if (!hooks_install()) {
        log_write("init aborted: hooks failed");
        log_shutdown();
        return FALSE;
    }
    log_write("timing hooks installed");

    speed_set_presets(cfg.presets, cfg.presetCount, cfg.startupSpeed);

    if (!control_start(g_hSelf, &cfg)) {
        log_write("init aborted: control threads failed");
        hooks_shutdown();
        log_shutdown();
        return FALSE;
    }

    log_write("ready: presets=%d startup=%.2f", cfg.presetCount, cfg.startupSpeed);
    return TRUE;
}
