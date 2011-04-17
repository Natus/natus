/*
 * Copyright (c) 2010 Nathaniel McCallum <nathaniel@natemccallum.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <cerrno>
#include "iocommon.hpp"

static Value fd_close(Value& fnc, Value& ths, Value& arg) {
	int fd = ths.getPrivate<long>(PRIV_POSIX_FD);
	if (close(fd) < 0)
		return throwException(ths, errno);
	return ths.newUndefined();
}

static Value fd_flush(Value& fnc, Value& ths, Value& arg) {
	int fd = ths.getPrivate<long>(PRIV_POSIX_FD);
	if (fsync(fd) < 0)
		return throwException(ths, errno);
	return ths.newUndefined();
}

static Value fd_read(Value& fnc, Value& ths, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "|n");

	int fd = ths.getPrivate<long>(PRIV_POSIX_FD);

	int bs = arg.get("length").to<int>() > 0 ? arg[0].to<int>() : 1024;
	char *buff = new char[bs];
	ssize_t rcvd = read(fd, buff, bs);
	if (rcvd < 0) {
		delete[] buff;
		return throwException(ths, errno);
	}
	UTF8 ret = UTF8(buff, rcvd);
	delete[] buff;
	return ths.newString(ret);
}

static UTF8 _readline(int fd) {
	char c = '\0';

	if (read(fd, &c, 1) == 0 || c == '\n')
		return "";

	return UTF8(&c, 1) + _readline(fd);
}

static Value fd_readLine(Value& fnc, Value& ths, Value& arg) {
	int fd = ths.getPrivate<long>(PRIV_POSIX_FD);
	return ths.newString(_readline(fd));
}

static Value fd_write(Value& fnc, Value& ths, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "s");

	int fd = ths.getPrivate<long>(PRIV_POSIX_FD);
	UTF8 buff = arg[0].to<UTF8>();
	ssize_t snt = write(fd, buff.c_str(), buff.length());
	if (snt < 0) return throwException(ths, errno);
	return ths.newNumber(snt);
}

static Value fd_writeLine(Value& fnc, Value& ths, Value& arg) {
	NATUS_CHECK_ARGUMENTS(arg, "s");

	int fd = ths.getPrivate<long>(PRIV_POSIX_FD);
	UTF8 buff = arg[0].to<UTF8>();
	buff += "\n";
	ssize_t snt = write(fd, buff.c_str(), buff.length());
	if (snt < 0) return throwException(ths, errno);
	return ths.newNumber(snt);
}

void stream_from_fd(Value& obj, int fd) {
	obj.setPrivate(PRIV_POSIX_FD, (void*) (size_t) fd);
	obj.set("close",         fd_close);
	obj.set("flush",         fd_flush);
	obj.set("read",          fd_read);
	obj.set("readLine",      fd_readLine);
	obj.set("write",         fd_write);
	obj.set("writeLine",     fd_writeLine);
}
