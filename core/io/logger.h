/*************************************************************************/
/*  logger.h                                                             */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#ifndef LOGGER_H
#define LOGGER_H

#include "core/os/file_access.h"
#include "core/vector.h"

class QChar;
#include <stdarg.h>

class Logger {
protected:
	bool should_log(bool p_err);

public:
	enum ErrorType {
		ERR_ERROR,
		ERR_WARNING,
		ERR_SCRIPT,
		ERR_SHADER
	};

	virtual void logv(const QChar *p_msg, bool p_err) = 0;
	virtual void logv(const char *p_msg, bool p_err) = 0;

	virtual void log_error(const char *p_function, const char *p_file, int p_line, const char *p_code, const char *p_rationale, ErrorType p_type = ERR_ERROR);

	void logf(const QString &p_msg);
    void logf(const char *p_msg);

	void logf_error(const QChar *p_msg);
	void logf_error(const char *msg);

	virtual ~Logger();
};

/**
 * Writes messages to stdout/stderr.
 */
class StdLogger : public Logger {

public:
	void logv(const QChar *p_msg, bool p_err) override;
	void logv(const char *p_msg, bool p_err) override;

	virtual ~StdLogger();
};

class CompositeLogger : public Logger {
	Vector<Logger *> loggers;

public:
	CompositeLogger(Vector<Logger *> p_loggers);

	void logv(const QChar *p_msg, bool p_err) override;
	void logv(const char *p_msg, bool p_err) override;
	virtual void log_error(const char *p_function, const char *p_file, int p_line, const char *p_code, const char *p_rationale, ErrorType p_type = ERR_ERROR);

	void add_logger(Logger *p_logger);

	virtual ~CompositeLogger();
};

#endif
