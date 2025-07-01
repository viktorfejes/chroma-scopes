#include "logger.h"

#include "macros.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

void logger_log(const char *func_name, uint32_t line, const char *msg, ...) {
    UNUSED(func_name);
    UNUSED(line);

    char out_msg[1024];
    char *p = out_msg;

    va_list args;
    va_start(args, msg);
    p += vsnprintf(p, 1024 - (p - out_msg), msg, args);
    va_end(args);

    p += snprintf(p, 1024 - (p - out_msg), "\n");
    printf("%s", out_msg);
}
