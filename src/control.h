#ifndef FNVSPD_CONTROL_H
#define FNVSPD_CONTROL_H

#include <windows.h>
#include "config.h"

/* Starts the hotkey poller and the control-file watcher threads.
 * Returns FALSE on failure; cleans up its own threads then. */
BOOL control_start(HMODULE hSelf, const Config* cfg);
void control_stop(void);

#endif
