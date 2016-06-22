/*
 * Copyright (c) 2015-16  David Lamparter, for NetDEF, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "qerr.h"
#include "vty.h"
#include "jhash.h"

static struct qerr last_error;

const struct qerr *qerr_get_last(qerr_r errval)
{
	if (last_error.kind == 0)
		return NULL;
	return &last_error;
}

qerr_r qerr_clear(void)
{
	memset(&last_error, 0, sizeof(last_error));
	return qerr_ok();
}

static qerr_r qerr_set_va(const char *file, int line, const char *func,
		enum qerr_kind kind, const char *pathname, int errno_val,
		const char *text, va_list va)
{
	last_error.file = file;
	last_error.line = line;
	last_error.func = func;
	last_error.kind = kind;

	last_error.unique_id = jhash(text, strlen(text),
			jhash(file, strlen(file), 0xd4ed0298));

	last_error.errno_val = errno_val;
	if (pathname)
		snprintf(last_error.pathname, sizeof(last_error.pathname),
				"%s", pathname);
	else
		last_error.pathname[0] = '\0';

	vsnprintf(last_error.message, sizeof(last_error.message), text, va);
	return -1;
}

qerr_r qerr_set_internal(const char *file, int line, const char *func,
		enum qerr_kind kind, const char *text, ...)
{
	qerr_r rv;
	va_list va;
	va_start(va, text);
	rv = qerr_set_va(file, line, func, kind, NULL, 0, text, va);
	va_end(va);
	return rv;
}

qerr_r qerr_set_internal_ext(const char *file, int line, const char *func,
		enum qerr_kind kind, const char *pathname, int errno_val,
		const char *text, ...)
{
	qerr_r rv;
	va_list va;
	va_start(va, text);
	rv = qerr_set_va(file, line, func, kind, pathname, errno_val, text, va);
	va_end(va);
	return rv;
}

#define REPLACE "$MSG"
void vty_print_error(struct vty *vty, qerr_r err, const char *msg, ...)
{
	char tmpmsg[256], *replacepos;

	va_list va;
	va_start(va, msg);
	vsnprintf(tmpmsg, sizeof(tmpmsg), msg, va);
	va_end(va);

	replacepos = strstr(tmpmsg, REPLACE);
	if (!replacepos)
		vty_out(vty, "%s%s", tmpmsg, VTY_NEWLINE);
	else {
		replacepos[0] = '\0';
		replacepos += sizeof(REPLACE) - 1;
		vty_out(vty, "%s%s%s%s",
			tmpmsg, last_error.message, replacepos, VTY_NEWLINE);
	}
}

