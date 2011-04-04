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

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "misc.h"
#include "value.h"

static ntValue *exception_toString(ntValue *ths, ntValue *fnc, ntValue *args) {
	size_t typelen, msglen, codelen;

	ntValue *type = nt_value_get_utf8(ths, "type");
	ntValue *msg  = nt_value_get_utf8(ths, "msg");
	ntValue *code = nt_value_get_utf8(ths, "code");

	ntChar *stype = nt_value_to_string_utf16(type, &typelen);
	ntChar *smsg  = nt_value_to_string_utf16(msg,  &msglen);
	ntChar *scode = nt_value_to_string_utf16(code, &codelen);
	bool hascode  = nt_value_is_undefined(code);

	nt_value_decref(type);
	nt_value_decref(msg);
	nt_value_decref(code);

	if (!stype || !smsg || !scode) {
		free(stype);
		free(smsg);
		free(scode);
		return NULL;
	}

	ntChar *str = calloc(typelen + msglen + codelen + 4, sizeof(ntChar));
	if (!str) {
		free(stype);
		free(smsg);
		free(scode);
		return NULL;
	}
	ntChar *tmp = str;

	memcpy(tmp, stype, sizeof(ntChar) * typelen); tmp += sizeof(ntChar) * typelen;
	memcpy(tmp, L": ", sizeof(ntChar) * 2);       tmp += sizeof(ntChar) * 2;
	if (hascode) {
		memcpy(tmp, scode, sizeof(ntChar) * codelen); tmp += sizeof(ntChar) * codelen;
		memcpy(tmp, L": ", sizeof(ntChar) * 2);       tmp += sizeof(ntChar) * 2;
	}
	memcpy(tmp, smsg, sizeof(ntChar) * msglen);

	ntValue *vstr = nt_value_new_string_utf16_length(ths, str, typelen + msglen + codelen + 4);
	free(stype);
	free(smsg);
	free(scode);
	free(str);
	return vstr;
}

char *nt_vsprintf(const char *format, va_list ap) {
	va_list apc;
	int size = 0;

	va_copy(apc, ap);
	size = vsnprintf(NULL, 0, format, apc);
	va_end(apc);

	char *buf = malloc(size+1);
	if (!size) return NULL;
	assert(size == vsnprintf(buf, size+1, format, ap));
	return buf;
}

char *nt_sprintf(const char *format, ...) {
	va_list ap;
	va_start(ap, format);
	char *tmp = nt_vsprintf(format, ap);
	va_end(ap);
	return tmp;
}

ntValue          *nt_throw_exception(const ntValue *ctx, const char *type, const char *format, ...) {
	va_list ap;
	va_start(ap, format);
	ntValue *exc = nt_throw_exception_varg(ctx, type, format, ap);
	va_end(ap);

	return exc;
}

ntValue *nt_throw_exception_varg(const ntValue *ctx, const char *type, const char *format, va_list ap) {
	char *msg = nt_vsprintf(format, ap);
	if (!msg) return NULL;

	ntValue *exc   = nt_value_new_object(ctx, NULL);
	ntValue *vtype = nt_value_new_string_utf8(ctx, type);
	ntValue *vmsg  = nt_value_new_string_utf8(ctx, msg);
	ntValue *vfunc = nt_value_new_function(ctx, exception_toString);

	nt_value_set_utf8(exc, "type",     vtype, ntPropAttrConstant);
	nt_value_set_utf8(exc, "msg",      vmsg,  ntPropAttrConstant);
	nt_value_set_utf8(exc, "toString", vfunc, ntPropAttrConstant);
	nt_value_to_exception(exc);

	free(msg);
	nt_value_decref(vtype);
	nt_value_decref(vmsg);
	nt_value_decref(vfunc);
	return exc;
}

ntValue          *nt_throw_exception_code (const ntValue *ctx, const char *type, int code, const char *format, ...) {
	va_list ap;
	va_start(ap, format);
	ntValue *exc = nt_throw_exception_code_varg(ctx, type, code, format, ap);
	va_end(ap);
	return exc;
}

ntValue          *nt_throw_exception_code_varg(const ntValue *ctx, const char *type, int code, const char *format, va_list ap) {
	ntValue *exc   = nt_throw_exception_varg(ctx, type, format, ap);
	ntValue *vcode = nt_value_new_number(ctx, (double) code);
	nt_value_set_utf8(exc, "code", vcode, ntPropAttrConstant);
	nt_value_decref(vcode);
	return exc;
}

ntValue          *nt_throw_exception_errno(const ntValue *ctx, int errorno) {
	const char* type = "OSError";
	switch (errorno) {
#ifdef ENOMEM
		case ENOMEM:
			return NULL;
#endif
#ifdef EPERM
		case EPERM:
			type = "PermissionError";
			break;
#endif
#ifdef ENOENT
		case ENOENT:
#endif
#ifdef ESRCH
		case ESRCH:
#endif
#ifdef EINTR
		case EINTR:
#endif
#ifdef EIO
		case EIO:
#endif
#ifdef ENXIO
		case ENXIO:
#endif
#ifdef E2BIG
		case E2BIG:
#endif
#ifdef ENOEXEC
		case ENOEXEC:
#endif
#ifdef EBADF
		case EBADF:
#endif
#ifdef ECHILD
		case ECHILD:
#endif
#ifdef EAGAIN
		case EAGAIN:
#endif
#ifdef EACCES
		case EACCES:
#endif
#ifdef EFAULT
		case EFAULT:
#endif
#ifdef ENOTBLK
		case ENOTBLK:
#endif
#ifdef EBUSY
		case EBUSY:
#endif
#ifdef EEXIST
		case EEXIST:
#endif
#ifdef EXDEV
		case EXDEV:
#endif
#ifdef ENODEV
		case ENODEV:
#endif
#ifdef ENOTDIR
		case ENOTDIR:
#endif
#ifdef EISDIR
		case EISDIR:
#endif
#ifdef EINVAL
		case EINVAL:
#endif
#ifdef ENFILE
		case ENFILE:
#endif
#ifdef EMFILE
		case EMFILE:
#endif
#ifdef ENOTTY
		case ENOTTY:
#endif
#ifdef ETXTBSY
		case ETXTBSY:
#endif
#ifdef EFBIG
		case EFBIG:
#endif
#ifdef ENOSPC
		case ENOSPC:
#endif
#ifdef ESPIPE
		case ESPIPE:
#endif
#ifdef EROFS
		case EROFS:
#endif
#ifdef EMLINK
		case EMLINK:
#endif
#ifdef EPIPE
		case EPIPE:
#endif
#ifdef EDOM
		case EDOM:
#endif
#ifdef ERANGE
		case ERANGE:
#endif
#ifdef EDEADLK
		case EDEADLK:
#endif
#ifdef ENAMETOOLONG
		case ENAMETOOLONG:
#endif
#ifdef ENOLCK
		case ENOLCK:
#endif
#ifdef ENOSYS
		case ENOSYS:
#endif
#ifdef ENOTEMPTY
		case ENOTEMPTY:
#endif
#ifdef ELOOP
		case ELOOP:
#endif
#ifdef ENOMSG
		case ENOMSG:
#endif
#ifdef EIDRM
		case EIDRM:
#endif
#ifdef ECHRNG
		case ECHRNG:
#endif
#ifdef EL2NSYNC
		case EL2NSYNC:
#endif
#ifdef EL3HLT
		case EL3HLT:
#endif
#ifdef EL3RST
		case EL3RST:
#endif
#ifdef ELNRNG
		case ELNRNG:
#endif
#ifdef EUNATCH
		case EUNATCH:
#endif
#ifdef ENOCSI
		case ENOCSI:
#endif
#ifdef EL2HLT
		case EL2HLT:
#endif
#ifdef EBADE
		case EBADE:
#endif
#ifdef EBADR
		case EBADR:
#endif
#ifdef EXFULL
		case EXFULL:
#endif
#ifdef ENOANO
		case ENOANO:
#endif
#ifdef EBADRQC
		case EBADRQC:
#endif
#ifdef EBADSLT
		case EBADSLT:
#endif
#ifdef EBFONT
		case EBFONT:
#endif
#ifdef ENOSTR
		case ENOSTR:
#endif
#ifdef ENODATA
		case ENODATA:
#endif
#ifdef ETIME
		case ETIME:
#endif
#ifdef ENOSR
		case ENOSR:
#endif
#ifdef ENONET
		case ENONET:
#endif
#ifdef ENOPKG
		case ENOPKG:
#endif
#ifdef EREMOTE
		case EREMOTE:
#endif
#ifdef ENOLINK
		case ENOLINK:
#endif
#ifdef EADV
		case EADV:
#endif
#ifdef ESRMNT
		case ESRMNT:
#endif
#ifdef ECOMM
		case ECOMM:
#endif
#ifdef EPROTO
		case EPROTO:
#endif
#ifdef EMULTIHOP
		case EMULTIHOP:
#endif
#ifdef EDOTDOT
		case EDOTDOT:
#endif
#ifdef EBADMSG
		case EBADMSG:
#endif
#ifdef EOVERFLOW
		case EOVERFLOW:
#endif
#ifdef ENOTUNIQ
		case ENOTUNIQ:
#endif
#ifdef EBADFD
		case EBADFD:
#endif
#ifdef EREMCHG
		case EREMCHG:
#endif
#ifdef ELIBACC
		case ELIBACC:
#endif
#ifdef ELIBBAD
		case ELIBBAD:
#endif
#ifdef ELIBSCN
		case ELIBSCN:
#endif
#ifdef ELIBMAX
		case ELIBMAX:
#endif
#ifdef ELIBEXEC
		case ELIBEXEC:
#endif
#ifdef EILSEQ
		case EILSEQ:
#endif
#ifdef ERESTART
		case ERESTART:
#endif
#ifdef ESTRPIPE
		case ESTRPIPE:
#endif
#ifdef EUSERS
		case EUSERS:
#endif
#ifdef ENOTSOCK
		case ENOTSOCK:
#endif
#ifdef EDESTADDRREQ
		case EDESTADDRREQ:
#endif
#ifdef EMSGSIZE
		case EMSGSIZE:
#endif
#ifdef EPROTOTYPE
		case EPROTOTYPE:
#endif
#ifdef ENOPROTOOPT
		case ENOPROTOOPT:
#endif
#ifdef EPROTONOSUPPORT
		case EPROTONOSUPPORT:
#endif
#ifdef ESOCKTNOSUPPORT
		case ESOCKTNOSUPPORT:
#endif
#ifdef EOPNOTSUPP
		case EOPNOTSUPP:
#endif
#ifdef EPFNOSUPPORT
		case EPFNOSUPPORT:
#endif
#ifdef EAFNOSUPPORT
		case EAFNOSUPPORT:
#endif
#ifdef EADDRINUSE
		case EADDRINUSE:
#endif
#ifdef EADDRNOTAVAIL
		case EADDRNOTAVAIL:
#endif
#ifdef ENETDOWN
		case ENETDOWN:
#endif
#ifdef ENETUNREACH
		case ENETUNREACH:
#endif
#ifdef ENETRESET
		case ENETRESET:
#endif
#ifdef ECONNABORTED
		case ECONNABORTED:
#endif
#ifdef ECONNRESET
		case ECONNRESET:
#endif
#ifdef ENOBUFS
		case ENOBUFS:
#endif
#ifdef EISCONN
		case EISCONN:
#endif
#ifdef ENOTCONN
		case ENOTCONN:
#endif
#ifdef ESHUTDOWN
		case ESHUTDOWN:
#endif
#ifdef ETOOMANYREFS
		case ETOOMANYREFS:
#endif
#ifdef ETIMEDOUT
		case ETIMEDOUT:
#endif
#ifdef ECONNREFUSED
		case ECONNREFUSED:
#endif
#ifdef EHOSTDOWN
		case EHOSTDOWN:
#endif
#ifdef EHOSTUNREACH
		case EHOSTUNREACH:
#endif
#ifdef EALREADY
		case EALREADY:
#endif
#ifdef EINPROGRESS
		case EINPROGRESS:
#endif
#ifdef ESTALE
		case ESTALE:
#endif
#ifdef EUCLEAN
		case EUCLEAN:
#endif
#ifdef ENOTNAM
		case ENOTNAM:
#endif
#ifdef ENAVAIL
		case ENAVAIL:
#endif
#ifdef EISNAM
		case EISNAM:
#endif
#ifdef EREMOTEIO
		case EREMOTEIO:
#endif
#ifdef EDQUOT
		case EDQUOT:
#endif
#ifdef ENOMEDIUM
		case ENOMEDIUM:
#endif
#ifdef EMEDIUMTYPE
		case EMEDIUMTYPE:
#endif
#ifdef ECANCELED
		case ECANCELED:
#endif
#ifdef ENOKEY
		case ENOKEY:
#endif
#ifdef EKEYEXPIRED
		case EKEYEXPIRED:
#endif
#ifdef EKEYREVOKED
		case EKEYREVOKED:
#endif
#ifdef EKEYREJECTED
		case EKEYREJECTED:
#endif
#ifdef EOWNERDEAD
		case EOWNERDEAD:
#endif
#ifdef ENOTRECOVERABLE
		case ENOTRECOVERABLE:
#endif
#ifdef ERFKILL
		case ERFKILL:
#endif
		default:
			break;
	}

	return nt_throw_exception_code(ctx, type, errorno, strerror(errorno));
}

ntValue          *nt_ensure_arguments      (const ntValue *arg, const char *fmt) {
	char types[4096];
	unsigned int len = nt_value_as_long(nt_value_get_utf8(arg, "length"));
	unsigned int minimum = 0;
	unsigned int i,j;
	for (i=0,j=0 ; i < len ; i++) {
		int depth = 0;
		bool correct = false;
		strcpy(types, "");

		if (minimum == 0 && fmt[j] == '|')
			minimum = j++;

		ntValue *thsArg = nt_value_get_index(arg, i);
		do {
			switch (fmt[j++]) {
				case 'a':
					correct = nt_value_get_type(thsArg) == ntValueTypeArray;
					if (strlen(types) + strlen("array, ") < 4095)
						strcat(types, "array, ");
					break;
				case 'b':
					correct = nt_value_get_type(thsArg) == ntValueTypeBool;
					if (strlen(types) + strlen("boolean, ") < 4095)
						strcat(types, "boolean, ");
					break;
				case 'f':
					correct = nt_value_get_type(thsArg) == ntValueTypeFunction;
					if (strlen(types) + strlen("function, ") < 4095)
						strcat(types, "function, ");
					break;
				case 'N':
					correct = nt_value_get_type(thsArg) == ntValueTypeNull;
					if (strlen(types) + strlen("null, ") < 4095)
						strcat(types, "null, ");
					break;
				case 'n':
					correct = nt_value_get_type(thsArg) == ntValueTypeNumber;
					if (strlen(types) + strlen("number, ") < 4095)
						strcat(types, "number, ");
					break;
				case 'o':
					correct = nt_value_get_type(thsArg) == ntValueTypeObject;
					if (strlen(types) + strlen("object, ") < 4095)
						strcat(types, "object, ");
					break;
				case 's':
					correct = nt_value_get_type(thsArg) == ntValueTypeString;
					if (strlen(types) + strlen("string, ") < 4095)
						strcat(types, "string, ");
					break;
				case 'u':
					correct = nt_value_get_type(thsArg) == ntValueTypeUndefined;
					if (strlen(types) + strlen("undefined, ") < 4095)
						strcat(types, "undefined, ");
					break;
				case '(':
					depth++;
					break;
				case ')':
					depth--;
					break;
				default:
					return nt_throw_exception(arg, "LogicError", "Invalid format character!");
			}

			nt_value_decref(thsArg);
		} while (!correct && depth > 0);

		if (strlen(types) > 2)
			types[strlen(types)-2] = '\0';

		if (strcmp(types, "") && !correct) {
			char msg[5120];
			snprintf(msg, 5120, "argument %u must be one of these types: %s", i, types);
			return nt_throw_exception(arg, "TypeError", msg);
		}
	}

	if (len < minimum) {
		char msg[1024];
		snprintf(msg, 1024, "Function requires at least %u arguments!", minimum);
		return nt_throw_exception(arg, "TypeError", msg);
	}

	return nt_value_new_undefined(arg);
}

ntValue          *nt_from_json            (const ntValue *json) {
	ntValue *glbl = nt_value_get_global(json);
	ntValue *JSON = nt_value_get_utf8(glbl, "JSON");
	ntValue *args = nt_value_new_array_builder(glbl, nt_value_incref((ntValue*) json));
	ntValue *rslt = nt_value_call_utf8(JSON, "parse", args);

	nt_value_decref(args);
	nt_value_decref(JSON);
	nt_value_decref(glbl);
	return rslt;
}

ntValue          *nt_from_json_utf8       (const ntValue *ctx, const    char *json, size_t len) {
	ntValue *JSON = nt_value_new_string_utf8_length(ctx, json, len);
	ntValue *rslt = nt_from_json(JSON);
	nt_value_decref(JSON);
	return rslt;
}

ntValue          *nt_from_json_utf16      (const ntValue *ctx, const  ntChar *json, size_t len) {
	ntValue *JSON = nt_value_new_string_utf16_length(ctx, json, len);
	ntValue *rslt = nt_from_json(JSON);
	nt_value_decref(JSON);
	return rslt;
}

ntValue          *nt_to_json              (const ntValue *val) {
	ntValue *glbl = nt_value_get_global(val);
	ntValue *JSON = nt_value_get_utf8(glbl, "JSON");
	ntValue *args = nt_value_new_array_builder(glbl, nt_value_incref((ntValue*) val));
	ntValue *rslt = nt_value_call_utf8(JSON, "stringify", args);

	nt_value_decref(args);
	nt_value_decref(JSON);
	nt_value_decref(glbl);
	return rslt;
}

char             *nt_to_json_utf8         (const ntValue *val, size_t *len) {
	ntValue *rslt = nt_to_json(val);
	char *srslt = nt_value_to_string_utf8(rslt, len);
	nt_value_decref(rslt);
	return srslt;
}

ntChar           *nt_to_json_utf16        (const ntValue *val, size_t *len) {
	ntValue *rslt = nt_to_json(val);
	ntChar *srslt = nt_value_to_string_utf16(rslt, len);
	nt_value_decref(rslt);
	return srslt;
}
