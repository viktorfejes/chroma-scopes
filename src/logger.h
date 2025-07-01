#pragma once

#include <stdint.h>

void logger_log(const char *func_name, uint32_t line, const char *msg, ...);

#ifndef LOG
    #define LOG(...) _LOG(__VA_ARGS__)
    #define _LOG(...) logger_log(__func__, __LINE__, __VA_ARGS__)
#endif
