/** @file src/os/error.c System dependant error messages. */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "types.h"
#include "strings.h"

#include "error.h"

#if defined(TOS) || defined(DOS)
bool g_consoleActive = true;
FILE * g_errlog = NULL;
#ifdef _DEBUG
FILE * g_outlog = NULL;
#endif
#endif

void Error(const char *format, ...) {
	va_list ap;

	va_start(ap, format);
#if defined(TOS) || defined(DOS)
	if(g_errlog) vfprintf(g_errlog, format, ap);
	if(g_consoleActive) vfprintf(stderr, format, ap);
#else
	vfprintf(stderr, format, ap);
#endif
	va_end(ap);
}

void Warning(const char *format, ...) {
	va_list ap;

	va_start(ap, format);
#if defined(TOS) || defined(DOS)
	if(g_errlog) vfprintf(g_errlog, format, ap);
	if(g_consoleActive) vfprintf(stderr, format, ap);
#else
	vfprintf(stderr, format, ap);
#endif
	va_end(ap);
}

#ifdef __3DS__
#include <3ds.h>
#endif

void TraceLog(const char *format, ...) {
	va_list ap;
	char buffer[512];
	va_start(ap, format);
	vsnprintf(buffer, sizeof(buffer), format, ap);
	va_end(ap);

#ifdef __3DS__
	svcOutputDebugString(buffer, strlen(buffer));
	FILE *f = fopen("sdmc:/3ds/dune3ds/trace.log", "a");
	if (f) {
		fprintf(f, "%s\n", buffer);
		fflush(f);
		fclose(f);
	}
#else
	fprintf(stderr, "%s\n", buffer);
#endif
}

#ifdef _DEBUG
void Debug(const char *format, ...) {
	va_list ap;

	va_start(ap, format);
#if defined(TOS) || defined(DOS)
	if(g_outlog) vfprintf(g_outlog, format, ap);
	if(g_consoleActive) vfprintf(stdout, format, ap);
#else
	vfprintf(stdout, format, ap);
#endif
	va_end(ap);
}
#endif
