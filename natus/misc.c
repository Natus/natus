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

#define _GNU_SOURCE
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stddef.h>

#include "misc.h"
#include "value.h"

#define SET_ARGUMENT(type) { \
  p = va_arg(apc, void*); \
  d = va_arg(apc, void*); \
  *((type*) p) = (natus_is_undefined(val) \
                   ? ((type) (intptr_t) d) \
                   : ((type) natus_to_double(val))); \
}

natusValue *
natus_throw_exception(const natusValue *ctx, const char *base, const char *name, const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  natusValue *exc = natus_throw_exception_varg(ctx, base, name, format, ap);
  va_end(ap);

  return exc;
}

natusValue *
natus_throw_exception_varg(const natusValue *ctx, const char *base, const char *name, const char *format, va_list ap)
{
  char *msg;
  if (vasprintf(&msg, format, ap) < 0)
    return NULL;

  natusValue *vmsg = natus_new_string_utf8(ctx, msg);
  free(msg);
  if (natus_is_exception(vmsg))
    return vmsg;

  natusValue *glb = natus_get_global(ctx);
  if (natus_is_exception(glb)) {
    natus_decref(vmsg);
    return glb;
  }

  // If the name passed in is an actual function, use it
  natusValue *err = natus_get_utf8(glb, name);
  if (!natus_is_function(err)) {
    natus_decref(err);

    // Try to get the base.  If base is not found, use Error().
    err = natus_get_utf8(glb, base);
    if (!natus_is_function(err)) {
      natus_decref(err);
      err = natus_get_utf8(glb, "Error");
    }
  }

  // Construct the exception
  natusValue *argv = natus_new_array(err, vmsg, NULL);
  natusValue *exc = natus_call_new_array(err, argv);
  natus_decref(argv);
  natus_decref(err);

  // Set the name
  natusValue *vname = natus_new_string_utf8(ctx, name);
  natus_decref(natus_set_utf8(exc, "name", vname, natusPropAttrNone));
  natus_decref(vname);

  return natus_to_exception(exc);
}

natusValue *
natus_throw_exception_code(const natusValue *ctx, const char *base, const char *name, int code, const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  natusValue *exc = natus_throw_exception_code_varg(ctx, base, name, code, format, ap);
  va_end(ap);
  return exc;
}

natusValue *
natus_throw_exception_code_varg(const natusValue *ctx, const char *base, const char *name, int code, const char *format, va_list ap)
{
  natusValue *exc = natus_throw_exception_varg(ctx, base, name, format, ap);
  natusValue *vcode = natus_new_number(ctx, (double) code);
  natus_decref(natus_set_utf8(exc, "code", vcode, natusPropAttrConstant));
  natus_decref(vcode);
  return exc;
}

natusValue *
natus_throw_exception_errno(const natusValue *ctx, int errorno)
{
  const char* base = NULL;
  const char* name = "OSError";
  switch (errorno) {
#ifdef ENOMEM
  case ENOMEM:
    return NULL;
#endif
#ifdef EPERM
  case EPERM:
    name = "PermissionError";
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

  return natus_throw_exception_code(ctx, base, name, errorno, strerror(errorno));
}

natusValue *
natus_ensure_arguments(natusValue *arg, const char *fmt)
{
  char types[4096];
  unsigned int len = natus_as_long(natus_get_utf8(arg, "length"));
  unsigned int minimum = 0;
  unsigned int i, j;
  for (i = 0, j = 0; i < len; i++) {
    int depth = 0;
    bool correct = false;
    strcpy(types, "");

    if (minimum == 0 && fmt[j] == '|')
      minimum = j++;

    natusValue *thsArg = natus_get_index(arg, i);
    do {
      switch (fmt[j++]) {
      case 'a':
        correct = natus_get_type(thsArg) == natusValueTypeArray;
        if (strlen(types) + strlen("array, ") < 4095)
          strcat(types, "array, ");
        break;
      case 'b':
        correct = natus_get_type(thsArg) == natusValueTypeBoolean;
        if (strlen(types) + strlen("boolean, ") < 4095)
          strcat(types, "boolean, ");
        break;
      case 'f':
        correct = natus_get_type(thsArg) == natusValueTypeFunction;
        if (strlen(types) + strlen("function, ") < 4095)
          strcat(types, "function, ");
        break;
      case 'N':
        correct = natus_get_type(thsArg) == natusValueTypeNull;
        if (strlen(types) + strlen("null, ") < 4095)
          strcat(types, "null, ");
        break;
      case 'n':
        correct = natus_get_type(thsArg) == natusValueTypeNumber;
        if (strlen(types) + strlen("number, ") < 4095)
          strcat(types, "number, ");
        break;
      case 'o':
        correct = natus_get_type(thsArg) == natusValueTypeObject;
        if (strlen(types) + strlen("object, ") < 4095)
          strcat(types, "object, ");
        break;
      case 's':
        correct = natus_get_type(thsArg) == natusValueTypeString;
        if (strlen(types) + strlen("string, ") < 4095)
          strcat(types, "string, ");
        break;
      case 'u':
        correct = natus_get_type(thsArg) == natusValueTypeUndefined;
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
        natus_decref(thsArg);
        return natus_throw_exception(arg, NULL, "SyntaxError", "Invalid format character!");
      }
    } while (!correct && depth > 0);
    natus_decref(thsArg);

    if (strlen(types) > 2)
      types[strlen(types) - 2] = '\0';

    if (strcmp(types, "") && !correct) {
      char msg[5120];
      snprintf(msg, 5120, "argument %u must be one of these types: %s", i, types);
      return natus_throw_exception(arg, NULL, "TypeError", msg);
    }
  }

  if (len < minimum) {
    return natus_throw_exception(arg, NULL, "TypeError", "Function requires at least %u arguments!", minimum);
  }

  return natus_new_undefined(arg);
}

natusValue *
natus_convert_arguments(natusValue *arg, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  natusValue *ret = natus_convert_arguments_varg(arg, fmt, ap);
  va_end(ap);
  return ret;
}

natusValue *
natus_convert_arguments_varg(natusValue *arg, const char *fmt, va_list ap)
{
  va_list apc;
  size_t i = 0;
  const char *c;

  va_copy(apc, ap);
  for (c = fmt; *c; c++) {
    if (*c != '%')
      continue;

    void *p;
    void *d;
    natusValue *val = natus_get_index(arg, i++);

    switch (*++c) {
    case 'h':
      switch (*++c) {
      case 'h':
        switch (*++c) {
        case 'd':
        case 'i':
          SET_ARGUMENT(signed char);
          break;
        case 'o':
        case 'u':
        case 'x':
        case 'X':
          SET_ARGUMENT(unsigned char);
          break;
        case 'n':
          break;
        default:
          goto inval;
        }
        break;
      case 'd':
      case 'i':
        SET_ARGUMENT(signed short int);
        break;
      case 'o':
      case 'u':
      case 'x':
      case 'X':
        SET_ARGUMENT(unsigned short int);
        break;
      case 'n':
        break;
      default:
        goto inval;
      }
      break;
    case 'j':
      switch (*++c) {
      case 'd':
      case 'i':
        SET_ARGUMENT(intmax_t);
        break;
      case 'o':
      case 'u':
      case 'x':
      case 'X':
        SET_ARGUMENT(uintmax_t);
        break;
      case 'n':
        break;
      default:
        goto inval;
      }
      break;
    case 'l':
      switch (*++c) {
      case 'l':
        switch (*++c) {
        case 'd':
        case 'i':
          SET_ARGUMENT(signed long long);
          break;
        case 'o':
        case 'u':
        case 'x':
        case 'X':
          SET_ARGUMENT(unsigned long long);
          break;
        case 'e':
        case 'f':
        case 'g':
        case 'E':
        case 'a':
          SET_ARGUMENT(long double);
          break;
        case 'n':
          break;
        default:
          goto inval;
        }
        break;
      case 'd':
      case 'i':
        SET_ARGUMENT(signed long int);
        break;
      case 'o':
      case 'u':
      case 'x':
      case 'X':
        SET_ARGUMENT(unsigned long int);
        break;
      case 'e':
      case 'f':
      case 'g':
      case 'E':
      case 'a':
        SET_ARGUMENT(double);
        break;
      case 'c':
        if (natus_is_string(val)) {
          p = va_arg(apc, void*);
          d = va_arg(apc, void*);
          size_t len = 0;
          natusChar *tmp = natus_to_string_utf16(val, &len);
          *((natusChar*) p) = len > 0 ? tmp[0] : (natusChar) 0;
          free(tmp);
          tmp = va_arg(apc, void*); // consume the default
        } else
          SET_ARGUMENT(natusChar);
        break;
      case 's':
        p = va_arg(apc, void*);
        d = va_arg(apc, void*);
        if (natus_is_undefined(val)) {
          if (d != NULL) {
            ssize_t len = 0;
            while (((natusChar*) d)[len++])
              ;
            natusChar *tmp = calloc(len+1, sizeof(natusChar));
            if (!tmp)
              goto nomem;
            for (; len >= 0; len--)
              tmp[len] = ((natusChar*) d)[len];
            d = tmp;
          }
          *((natusChar**) p) = (natusChar*) d;
        } else
          *((natusChar**) p) = (natusChar*) natus_to_string_utf16(val, NULL);
        break;
      case 'n':
        break;
      default:
        goto inval;
      }
      break;
    case 'L':
    case 'q':
      switch (*++c) {
      case 'd':
      case 'i':
        SET_ARGUMENT(signed long long);
        break;
      case 'o':
      case 'u':
      case 'x':
      case 'X':
        SET_ARGUMENT(unsigned long long);
        break;
      case 'e':
      case 'f':
      case 'g':
      case 'E':
      case 'a':
        SET_ARGUMENT(long double);
        break;
      case 'n':
        break;
      default:
        goto inval;
      }
      break;
    case 't':
      switch (*++c) {
      case 'd':
      case 'i':
      case 'o':
      case 'u':
      case 'x':
      case 'X':
        SET_ARGUMENT(ptrdiff_t);
        break;
      case 'n':
        break;
      default:
        goto inval;
      }
      break;
    case 'z':
      switch (*++c) {
      case 'd':
      case 'i':
        SET_ARGUMENT(ssize_t);
        break;
      case 'o':
      case 'u':
      case 'x':
      case 'X':
        SET_ARGUMENT(size_t);
        break;
      case 'n':
        break;
      default:
        goto inval;
      }
      break;
    case '%':
      break;
    case 'd':
    case 'i':
      SET_ARGUMENT(int);
      break;
    case 'D':
      SET_ARGUMENT(signed long int);
      break;
    case 'o':
    case 'u':
    case 'x':
    case 'X':
      SET_ARGUMENT(unsigned int);
      break;
    case 'e':
    case 'f':
    case 'g':
    case 'E':
    case 'a':
      SET_ARGUMENT(float);
      break;
    case 'c':
      if (natus_is_string(val)) {
        p = va_arg(apc, void*);
        d = va_arg(apc, void*);

        size_t len = 0;
        char *tmp = natus_to_string_utf8(val, &len);
        *((char*) p) = (tmp && len > 0) ? tmp[0] : (char) 0;
        free(tmp);
      } else
        SET_ARGUMENT(char);
      break;
    case 's':
      p = va_arg(apc, void*);
      d = va_arg(apc, void*);
      if (natus_is_undefined(val))
        *((char**) p) = strdup((char*) d);
      else
        *((char**) p) = (char*) natus_to_string_utf8(val, NULL);
      break;
    case '[':
      p = va_arg(apc, void*);
      d = va_arg(apc, void*);
      if (!strchr(++c, ']'))
        goto inval;
      if (!natus_is_undefined(val)) {
        char *tmp = strdup(c);
        if (!tmp)
          goto nomem;
        *strchr(tmp, ']') = '\0';
        *((void**) p) = natus_get_private_name(val, tmp);
        free(tmp);
      } else
        *((void**) p) = d;
      c = strchr(c, ']');
      break;
    case 'n':
      break;
    default:
      goto inval;
    }

    natus_decref(val);
    continue;

  inval:
    va_end(ap);
    natus_decref(val);
    return natus_throw_exception(arg, NULL, "SyntaxError", "Invalid format string!");

  nomem:
    va_end(ap);
    natus_decref(val);
    return NULL;
  }

  va_end(apc);
  return natus_new_number(arg, i);
}
