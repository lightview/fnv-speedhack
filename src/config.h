#ifndef FNVSPD_CONFIG_H
#define FNVSPD_CONFIG_H

#include <windows.h>
#include "hooks.h"

#define FNVSPD_MAX_SPEEDKEYS 8

typedef struct {
    WORD  vk;                  /* virtual key code */
    float speed;               /* speed applied when pressed */
} SpeedKey;

typedef struct {
    WORD  vkCycle;             /* VK code: cycle through presets (default F8)  */
    WORD  vkUp;                /* VK code: step up (default F9), 0 = disabled  */
    WORD  vkDown;              /* VK code: step down (default F10), 0 = off    */
    SpeedKey speedKeys[FNVSPD_MAX_SPEEDKEYS]; /* direct "key -> speed" binds */
    int   speedKeyCount;
    char  padModifier[32];     /* gamepad combo modifier, e.g. "LB+RB"; "" = off */
    float padA, padB, padX, padY; /* face button speeds, 0 = disabled */
    float presets[FNVSPD_MAX_PRESETS];
    int   presetCount;
    float startupSpeed;
    int   controlFileEnabled;  /* fnvspeed.txt / fnvspeed_status.txt IPC       */
    DWORD hotkeyPollMs;
    DWORD controlPollMs;
    int   beepFeedback;
    int   logEnabled;
} Config;

/* Reads FNVSpeedhack.ini (same dir as the DLL), applying defaults. */
void cfg_load(const char* dllDir, Config* cfg);

#endif
