#pragma once
#include <stdbool.h>
#include <stddef.h>
bool fap_console_init(void);
bool fap_console_next(char *command, size_t size);
bool fap_console_binary_busy(void);
/* Bounded, whole-frame USB writes, serialized with snapshot transfers. */
bool fap_console_printf(const char *format, ...) __attribute__((format(printf, 1, 2)));
