#ifndef FNVSPD_LOG_H
#define FNVSPD_LOG_H

/* Simple logger: writes fnvspeed.log next to the plugin DLL. */

void log_init(const char* dllDir, int enabled);
void log_write(const char* fmt, ...);
void log_shutdown(void);

#endif
