#ifndef FNVSPD_HOOKS_H
#define FNVSPD_HOOKS_H

#include <windows.h>

#define FNVSPD_MAX_PRESETS 16

/* Install MinHook hooks on the Windows timing APIs.
 * Returns FALSE and leaves nothing installed on failure. */
BOOL  hooks_install(void);
void  hooks_shutdown(void);

/* Unscaled millisecond clock, safe to call from anywhere (for logging). */
DWORD hooks_real_time_ms(void);

/* Speed engine. All functions are thread-safe. */
void   speed_set_presets(const float* presets, int count, float startupSpeed);
double speed_set(double v);      /* returns applied value, clamped */
double speed_cycle(void);        /* next preset, wraps around */
double speed_step(int dir);      /* move through presets, clamped at ends */
double speed_get(void);
int    speed_preset_index(void); /* -1 when on a custom value */
float* speed_presets(int* count);

#endif
