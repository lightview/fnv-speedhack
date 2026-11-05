#ifndef FNVSPD_XINPUT_PAD_H
#define FNVSPD_XINPUT_PAD_H

#include <windows.h>
#include "config.h"

BOOL pad_init(const Config* cfg);
void pad_tick(void (*on_speed)(double v, const char* source));

#endif
