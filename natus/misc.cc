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
#include <cstring>

#include <sstream>

#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#include "natus.h"
namespace natus {

Value fromJSON(const Value& ctx, string json) {
	vector<Value> args;
	args.push_back(ctx.newString(json));
	Value obj = ctx.getGlobal().get("JSON");
	return obj.call("parse", args);
}

string toJSON(const Value& val) {
	vector<Value> args;
	args.push_back(val);
	Value obj = val.getGlobal().get("JSON");
	return obj.call("stringify", args).toString();
}

static Value exception_toString(Value& ths, Value& fnc, vector<Value>& arg) {
	string type = ths.get("type").toString();
	string msg  = ths.get("msg").toString();
	Value  code = ths.get("code");

	string output = type + ": ";
	if (!code.isUndefined())
		output += code.toString() + ": ";
	output += msg;
	return ths.newString(output);
}

Value throwException(const Value& ctx, string type, string message) {
	Value exc = ctx.newObject();
	exc.set("type",     type);
	exc.set("msg",      message);
	exc.set("toString", exception_toString);
	return exc.toException();
}

Value throwException(const Value& ctx, string type, string message, long code) {
	Value exc = throwException(ctx, type, message);
	exc.set("code", (double) code);
	return exc;
}

Value throwException(const Value& ctx, int errorno) {
	const char* type = "OSError";
	switch (errorno) {
		case ENOMEM:
			return NULL;
		case EPERM:
			type = "PermissionError";
			break;
		case ENOENT:
		case ESRCH:
		case EINTR:
		case EIO:
		case ENXIO:
		case E2BIG:
		case ENOEXEC:
		case EBADF:
		case ECHILD:
		case EAGAIN:
		case EACCES:
		case EFAULT:
		case ENOTBLK:
		case EBUSY:
		case EEXIST:
		case EXDEV:
		case ENODEV:
		case ENOTDIR:
		case EISDIR:
		case EINVAL:
		case ENFILE:
		case EMFILE:
		case ENOTTY:
		case ETXTBSY:
		case EFBIG:
		case ENOSPC:
		case ESPIPE:
		case EROFS:
		case EMLINK:
		case EPIPE:
		case EDOM:
		case ERANGE:
		case EDEADLK:
		case ENAMETOOLONG:
		case ENOLCK:
		case ENOSYS:
		case ENOTEMPTY:
		case ELOOP:
		case ENOMSG:
		case EIDRM:
		case ECHRNG:
		case EL2NSYNC:
		case EL3HLT:
		case EL3RST:
		case ELNRNG:
		case EUNATCH:
		case ENOCSI:
		case EL2HLT:
		case EBADE:
		case EBADR:
		case EXFULL:
		case ENOANO:
		case EBADRQC:
		case EBADSLT:
		case EBFONT:
		case ENOSTR:
		case ENODATA:
		case ETIME:
		case ENOSR:
		case ENONET:
		case ENOPKG:
		case EREMOTE:
		case ENOLINK:
		case EADV:
		case ESRMNT:
		case ECOMM:
		case EPROTO:
		case EMULTIHOP:
		case EDOTDOT:
		case EBADMSG:
		case EOVERFLOW:
		case ENOTUNIQ:
		case EBADFD:
		case EREMCHG:
		case ELIBACC:
		case ELIBBAD:
		case ELIBSCN:
		case ELIBMAX:
		case ELIBEXEC:
		case EILSEQ:
		case ERESTART:
		case ESTRPIPE:
		case EUSERS:
		case ENOTSOCK:
		case EDESTADDRREQ:
		case EMSGSIZE:
		case EPROTOTYPE:
		case ENOPROTOOPT:
		case EPROTONOSUPPORT:
		case ESOCKTNOSUPPORT:
		case EOPNOTSUPP:
		case EPFNOSUPPORT:
		case EAFNOSUPPORT:
		case EADDRINUSE:
		case EADDRNOTAVAIL:
		case ENETDOWN:
		case ENETUNREACH:
		case ENETRESET:
		case ECONNABORTED:
		case ECONNRESET:
		case ENOBUFS:
		case EISCONN:
		case ENOTCONN:
		case ESHUTDOWN:
		case ETOOMANYREFS:
		case ETIMEDOUT:
		case ECONNREFUSED:
		case EHOSTDOWN:
		case EHOSTUNREACH:
		case EALREADY:
		case EINPROGRESS:
		case ESTALE:
		case EUCLEAN:
		case ENOTNAM:
		case ENAVAIL:
		case EISNAM:
		case EREMOTEIO:
		case EDQUOT:
		case ENOMEDIUM:
		case EMEDIUMTYPE:
		case ECANCELED:
		case ENOKEY:
		case EKEYEXPIRED:
		case EKEYREVOKED:
		case EKEYREJECTED:
		case EOWNERDEAD:
		case ENOTRECOVERABLE:
		case ERFKILL:
		default:
			break;
	}

	return throwException(ctx, type, strerror(errorno), errorno);
}

Value checkArguments(const Value& ctx, const vector<Value>& arg, const char* fmt) {
	size_t len = arg.size();

	bool minimum = 0;
	for (size_t i=0,j=0 ; i < len ; i++) {
		int depth = 0;
		bool correct = false;
		string types = "";

		if (minimum == 0 && fmt[j] == '|')
			minimum = j++;

		do {
			switch (fmt[j++]) {
				case 'a':
					correct = arg[i].isArray();
					types += "array, ";
					break;
				case 'b':
					correct = arg[i].isBool();
					types += "boolean, ";
					break;
				case 'f':
					correct = arg[i].isFunction();
					types += "function, ";
					break;
				case 'N':
					correct = arg[i].isNull();
					types += "null, ";
					break;
				case 'n':
					correct = arg[i].isNumber();
					types += "number, ";
					break;
				case 'o':
					correct = arg[i].isObject();
					types += "object, ";
					break;
				case 's':
					correct = arg[i].isString();
					types += "string, ";
					break;
				case 'u':
					correct = arg[i].isUndefined();
					types += "undefined, ";
					break;
				case '(':
					depth++;
					break;
				case ')':
					depth--;
					break;
				default:
					return throwException(ctx, "LogicError", "Invalid format character!");
			}
		} while (!correct && depth > 0);

		if (types.length() > 2)
			types = types.substr(0, types.length()-2);

		if (types != "" && !correct) {
			stringstream msg;
			msg << "argument " << i << " must be one of these types: " << types;
			return throwException(ctx, "TypeError", msg.str());
		}
	}

	if (len < minimum) {
		stringstream out;
		out << minimum;
		return throwException(ctx, "TypeError", "Function requires at least " + out.str() + " arguments!");
	}

	return ctx.newUndefined();
}

}
