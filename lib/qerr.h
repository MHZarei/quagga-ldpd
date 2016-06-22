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

#ifndef _QUAGGA_QERR_H
#define _QUAGGA_QERR_H

/***********************************************************
 * scroll down to the end of this file for a full example! *
 ***********************************************************/

#include <stdint.h>
#include <limits.h>
#include <errno.h>

/* return type when this error indication stuff is used.
 *
 * guaranteed to have boolean evaluation to "false" when OK, "true" when error
 * (i.e. can be changed to pointer in the future if neccessary)
 *
 * For checking, always use "if (value)", nothing else.
 * Do _NOT_ use any integer constant (!= 0), or sign check (< 0).
 */
typedef int qerr_r;

/* rough category of error indication */
enum qerr_kind {
	/* no error */
	QERR_OK = 0,

	/* something isn't the way it's supposed to be.
	 * (things that might otherwise be asserts, really)
	 */
	QERR_CODE_BUG,

	/* user-supplied parameters don't make sense or is inconsistent
	 * if you can express a rule for it (e.g. "holdtime > 2 * keepalive"),
	 * it's this category.
	 */
	QERR_CONFIG_INVALID,

	/* user-supplied parameters don't line up with reality
	 * (IP address or interface not available, etc.)
	 * NB: these are really TODOs where the code needs to be fixed to
	 * respond to future changes!
	 */
	QERR_CONFIG_REALITY,

	/* out of some system resource (probably memory)
	 * aka "you didn't spend enough money error" */
	QERR_RESOURCE,

	/* system error (permission denied, etc.) */
	QERR_SYSTEM,

	/* error return from some external library
	 * (QERR_SYSTEM and QERR_LIBRARY are not strongly distinct) */
	QERR_LIBRARY,
};

struct qerr {
	/* code location */
	const char *file;
	const char *func;
	int line;

	enum qerr_kind kind;

	/* unique_id is calculated as a checksum of source filename and error
	 * message format (*before* calling vsnprintf).  Line number and
	 * function name are not used; this keeps the number reasonably static
	 * across changes.
	 */
	uint32_t unique_id;

	char message[192];

	/* valid if != 0.  note "errno" might be preprocessor foobar. */
	int errno_val;
	/* valid if pathname[0] != '\0' */
	char pathname[PATH_MAX];
};

/* get error details.
 *
 * NB: errval/qerr_r does NOT carry the full error information.  It's only
 * passed around for future API flexibility.  qerr_get_last always returns
 * the last error set in the current thread.  (If Quagga goes multithreaded,
 * this will be thread-local.)
 */
const struct qerr *qerr_get_last(qerr_r errval);

/* can optionally be called at strategic locations.
 * always returns 0. */
qerr_r qerr_clear(void);

/* do NOT call these functions directly.  only for macro use! */
qerr_r qerr_set_internal(const char *file, int line, const char *func,
		enum qerr_kind kind, const char *text, ...);
qerr_r qerr_set_internal_ext(const char *file, int line, const char *func,
		enum qerr_kind kind, const char *pathname, int errno_val,
		const char *text, ...);

#define qerr_ok() \
	0

/* Report an error.
 *
 * If you need to do cleanup (free memory, etc.), save the return value in a
 * variable of type qerr_r.
 *
 * Don't put a \n (or VTY_NEWLINE) at the end of the error message.
 */
#define qerr_code_bug(...) \
	qerr_set_internal(__FILE__, __LINE__, __func__, QERR_CODE_BUG, \
			__VA_ARGS__)
#define qerr_config_invalid(...) \
	qerr_set_internal(__FILE__, __LINE__, __func__, QERR_CONFIG_INVALID, \
			__VA_ARGS__)
#define qerr_config_reality(...) \
	qerr_set_internal(__FILE__, __LINE__, __func__, QERR_CONFIG_REALITY, \
			__VA_ARGS__)
#define qerr_config_resource(...) \
	qerr_set_internal(__FILE__, __LINE__, __func__, QERR_RESOURCE, \
			__VA_ARGS__)
#define qerr_system(...) \
	qerr_set_internal(__FILE__, __LINE__, __func__, QERR_SYSTEM, \
			__VA_ARGS__)
#define qerr_library(...) \
	qerr_set_internal(__FILE__, __LINE__, __func__, QERR_LIBRARY, \
			__VA_ARGS__)

/* extended information variants */
#define qerr_system_errno(...) \
	qerr_set_internal_ext(__FILE__, __LINE__, __func__, QERR_SYSTEM, \
			NULL, errno, __VA_ARGS__)
#define qerr_system_path_errno(path, ...) \
	qerr_set_internal_ext(__FILE__, __LINE__, __func__, QERR_SYSTEM, \
			path, errno, __VA_ARGS__)

#include "vty.h"
/* print error message to vty;  $ERR is replaced by the error's message */
void vty_print_error(struct vty *vty, qerr_r err, const char *msg, ...);

#define CMD_QERR_DO(func, action, ...) \
	do { qerr_r cmd_retval = func; \
		if (cmd_retval) { \
			vty_print_error(vty, cmd_retval, __VA_ARGS__); \
			action; \
		} \
	} while (0)

#define CMD_QERR_RETURN(func, ...) \
	CMD_QERR_DO(func, return CMD_WARNING, __VA_ARGS__)
#define CMD_QERR_GOTO(func, label, ...) \
	CMD_QERR_DO(func, goto label, __VA_ARGS__)

/* example:

qerr_r foo_bar_set(struct object *obj, int bar)
{
	if (bar < 1 || bar >= 100)
		return qerr_config_invalid("bar setting (%d) must be 0<x<100", bar);
	obj->bar = bar;
	if (ioctl (obj->fd, bar))
		return qerr_system_errno("couldn't set bar to %d", bar);

	return qerr_ok();
}

DEFUN("bla")
{
	CMD_QERR_RETURN(foo_bar_set(obj, atoi(argv[1])),
		"command failed: $ERR\n");
	return CMD_SUCCESS;
}

*/

#endif /* _QERR_H */
