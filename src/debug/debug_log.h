#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

void debug_log_init(const char* path);
void debug_log_close(void);
void debug_logf(const char* fmt, ...);
void debug_log_install_signal_handlers(void);

#endif /* DEBUG_LOG_H */
