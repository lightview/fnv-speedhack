/*
 * Gamepad speed combos - read the controller directly through XInput,
 * the same way the game does. This makes speed switching work with
 * physical buttons and no Steam Input bindings at all.
 *
 * Default combos: LB+RB held, then
 *   A = 1x, B = 3x, X = 5x, Y = 10x
 * All configurable via FNVSpeedhack.ini (PadModifier / PadA..PadY).
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <xinput.h>
#include <stdio.h>
#include <string.h>
#include "xinput_pad.h"
#include "config.h"
#include "hooks.h"

static HMODULE  g_xi = NULL;
static DWORD (WINAPI *pXInputGetState)(DWORD, XINPUT_STATE*) = NULL;
static DWORD g_padIndex = 0;
static BOOL  g_found = FALSE;

typedef struct {
    DWORD mask;
    float speed;
    const char* label;
} PadAction;

static PadAction g_actions[4];
static int g_actionCount = 0;
static DWORD g_modifier = 0;
static DWORD g_prevFace = 0;
static BOOL g_prevModHeld = FALSE;

typedef struct { const char* name; DWORD mask; } BtnName;
static const BtnName BTN_NAMES[] = {
    { "LB", XINPUT_GAMEPAD_LEFT_SHOULDER },
    { "RB", XINPUT_GAMEPAD_RIGHT_SHOULDER },
    { "A", XINPUT_GAMEPAD_A },
    { "B", XINPUT_GAMEPAD_B },
    { "X", XINPUT_GAMEPAD_X },
    { "Y", XINPUT_GAMEPAD_Y },
    { "BACK", XINPUT_GAMEPAD_BACK },
    { "START", XINPUT_GAMEPAD_START },
    { "UP", XINPUT_GAMEPAD_DPAD_UP },
    { "DOWN", XINPUT_GAMEPAD_DPAD_DOWN },
    { "LEFT", XINPUT_GAMEPAD_DPAD_LEFT },
    { "RIGHT", XINPUT_GAMEPAD_DPAD_RIGHT },
    { "L3", XINPUT_GAMEPAD_LEFT_THUMB },
    { "R3", XINPUT_GAMEPAD_RIGHT_THUMB },
};

static DWORD parse_mask(const char* s)
{
    char token[16];
    int ti = 0;
    DWORD mask = 0;
    size_t i, len = strlen(s);

    for (i = 0; i <= len; i++) {
        char c = s[i];
        if (c == '+' || c == ',' || c == 0) {
            token[ti] = 0;
            if (ti > 0) {
                unsigned j;
                for (j = 0; j < sizeof(BTN_NAMES) / sizeof(BTN_NAMES[0]); j++) {
                    if (!_stricmp(token, BTN_NAMES[j].name))
                        mask |= BTN_NAMES[j].mask;
                }
            }
            ti = 0;
        } else if (ti < sizeof(token) - 1) {
            token[ti++] = c;
        }
    }
    return mask;
}

BOOL pad_init(const Config* cfg)
{
    int i;

    g_xi = LoadLibraryA("xinput1_3.dll");
    if (!g_xi)
        g_xi = LoadLibraryA("xinput1_4.dll");
    if (!g_xi)
        return FALSE;
    pXInputGetState = (DWORD(WINAPI*)(DWORD, XINPUT_STATE*))
        (FARPROC)GetProcAddress(g_xi, "XInputGetState");
    if (!pXInputGetState)
        return FALSE;

    g_modifier = parse_mask(cfg->padModifier);
    if (g_modifier == 0)
        return FALSE; /* combos disabled */

    g_actionCount = 0;
    if (cfg->padA > 0.0f) { g_actions[g_actionCount].mask = XINPUT_GAMEPAD_A; g_actions[g_actionCount].speed = cfg->padA; g_actions[g_actionCount].label = "A"; g_actionCount++; }
    if (cfg->padB > 0.0f) { g_actions[g_actionCount].mask = XINPUT_GAMEPAD_B; g_actions[g_actionCount].speed = cfg->padB; g_actions[g_actionCount].label = "B"; g_actionCount++; }
    if (cfg->padX > 0.0f) { g_actions[g_actionCount].mask = XINPUT_GAMEPAD_X; g_actions[g_actionCount].speed = cfg->padX; g_actions[g_actionCount].label = "X"; g_actionCount++; }
    if (cfg->padY > 0.0f) { g_actions[g_actionCount].mask = XINPUT_GAMEPAD_Y; g_actions[g_actionCount].speed = cfg->padY; g_actions[g_actionCount].label = "Y"; g_actionCount++; }

    for (i = 0; i < g_actionCount; i++)
        g_actions[i].mask &= ~g_modifier;

    return g_actionCount > 0;
}

void pad_tick(void (*on_speed)(double v, const char* source))
{
    XINPUT_STATE state;
    DWORD face, i;
    BOOL modHeld;

    if (!pXInputGetState)
        return;

    ZeroMemory(&state, sizeof(state));
    /* first connected pad wins; re-scan only while we have none */
    if (!g_found) {
        DWORD idx;
        for (idx = 0; idx < 4; idx++) {
            if (pXInputGetState(idx, &state) == ERROR_SUCCESS) {
                g_padIndex = idx;
                g_found = TRUE;
                break;
            }
        }
        if (!g_found)
            return;
    }

    if (pXInputGetState(g_padIndex, &state) != ERROR_SUCCESS) {
        g_found = FALSE; /* disconnected, re-scan next time */
        g_prevModHeld = FALSE;
        g_prevFace = 0;
        return;
    }

    face = state.Gamepad.wButtons;
    modHeld = (g_modifier != 0) && ((face & g_modifier) == g_modifier);

    if (modHeld && g_prevModHeld) {
        for (i = 0; i < (DWORD)g_actionCount; i++) {
            DWORD now = face & g_actions[i].mask;
            DWORD before = g_prevFace & g_actions[i].mask;
            if (now && !before) {
                char why[32];
                _snprintf(why, sizeof(why), "pad %s+%s",
                          g_modifier ? "" : "", g_actions[i].label);
                on_speed((double)g_actions[i].speed, why);
            }
        }
    }

    if (!modHeld && g_prevModHeld)
        g_prevFace = 0; /* re-arm after releasing the modifier */
    else
        g_prevFace = face;
    g_prevModHeld = modHeld;
}
