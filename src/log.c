#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "shared.h"

// internal stuff below

enum XAVA_MESSAGE_TYPE {
	XAVA_LOG_ERROR, XAVA_LOG_WARN, XAVA_LOG_NORM, XAVA_LOG_SPAM
};

static void __internal_xavaMsgHnd(enum XAVA_MESSAGE_TYPE mes, const char *fmt,
		const char *func, const char *file, int line, va_list list) {

	// please don't let this be a memory bug
	char *newFmt = malloc((strlen(fmt)+100)*sizeof(char));

	FILE *output = stdout;

	// process message headers
	switch(mes) {
		case XAVA_LOG_ERROR:
			sprintf(newFmt, "[ERROR] %s at %s:%d - %s\n", func, file, line, fmt);
			output = stderr;
			break;
		case XAVA_LOG_WARN:
			sprintf(newFmt, "[WARN] %s at %s:%d - %s\n", func, file, line, fmt);
			break;
		case XAVA_LOG_NORM:
			sprintf(newFmt, "[INFO] %s at %s:%d - %s\n", func, file, line, fmt);
			break;
		case XAVA_LOG_SPAM:
			sprintf(newFmt, "[SPAM] %s at %s:%d - %s\n", func, file, line, fmt);
			break;
	}

	vfprintf(output, newFmt, list);

	free(newFmt);
}


// global stuff below

EXP_FUNC void __internal_xavaSpam(const char *func, const char *file, int line, const char *fmt, ...) {
	va_list list;
	va_start(list, fmt);
	__internal_xavaMsgHnd(XAVA_LOG_SPAM, fmt, func, file, line, list);
	va_end(list);
}

EXP_FUNC void __internal_xavaLog(const char *func, const char *file, int line, const char *fmt, ...) {
	va_list list;
	va_start(list, fmt);
	__internal_xavaMsgHnd(XAVA_LOG_NORM, fmt, func, file, line, list);
	va_end(list);
}

EXP_FUNC void __internal_xavaWarn(const char *func, const char *file, int line, const char *fmt, ...) {
	va_list list;
	va_start(list, fmt);
	__internal_xavaMsgHnd(XAVA_LOG_WARN, fmt, func, file, line, list);
	va_end(list);
}

EXP_FUNC void __internal_xavaError(const char *func, const char *file, int line, const char *fmt, ...) {
	va_list list;
	va_start(list, fmt);
	__internal_xavaMsgHnd(XAVA_LOG_ERROR, fmt, func, file, line, list);
	va_end(list);
}

EXP_FUNC void __internal_xavaDie(void) {
	exit(EXIT_FAILURE);
}

