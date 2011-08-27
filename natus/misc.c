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
  *((type*) p) = (nt_value_is_undefined(val) \
                   ? ((type) (intptr_t) d) \
                   : ((type) nt_value_to_double(val))); \
}

ntValue *
nt_throw_exception(const ntValue *ctx, const char *base, const char *name, const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  ntValue *exc = nt_throw_exception_varg (ctx, base, name, format, ap);
  va_end(ap);

  return exc;
}

ntValue *
nt_throw_exception_varg(const ntValue *ctx, const char *base, const char *name, const char *format, va_list ap)
{
  char *msg;
  if (vasprintf(&msg, format, ap) < 0)
    return NULL;

  ntValue *vmsg = nt_value_new_string_utf8(ctx, msg);
  free(msg);
  if (nt_value_is_exception(vmsg))
    return vmsg;

  ntValue *glb = nt_value_get_global(ctx);
  if (nt_value_is_exception(glb)) {
    nt_value_decref(vmsg);
    return glb;
  }

  // If the name passed in is an actual function, use it
  ntValue *err = nt_value_get_utf8(glb, name);
  if (!nt_value_is_function(err)) {
    nt_value_decref(err);

    // Try to get the base.  If base is not found, use Error().
    err = nt_value_get_utf8(glb, base);
    if (!nt_value_is_function(err)) {
      nt_value_decref(err);
      err = nt_value_get_utf8(glb, "Error");
    }
  }

  // Construct the exception
  ntValue *argv = nt_value_new_array(err, vmsg, NULL);
  ntValue *exc = nt_value_call_new(err, argv);
  nt_value_decref(argv);
  nt_value_decref(err);

  // Set the name
  ntValue *vname = nt_value_new_string_utf8(ctx, name);
  nt_value_decref(nt_value_set_utf8(exc, "name", vname, ntPropAttrNone));
  nt_value_decref(vname);

  return nt_value_to_exception(exc);
}

ntValue *
nt_throw_exception_code(const ntValue *ctx, const char *base, const char *name, int code, const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  ntValue *exc = nt_throw_exception_code_varg (ctx, base, name, code, format, ap);
  va_end(ap);
  return exc;
}

ntValue *
nt_throw_exception_code_varg(const ntValue *ctx, const char *base, const char *name, int code, const char *format, va_list ap)
{
  ntValue *exc = nt_throw_exception_varg(ctx, base, name, format, ap);
  ntValue *vcode = nt_value_new_number(ctx, (double) code);
  nt_value_decref(nt_value_set_utf8(exc, "code", vcode, ntPropAttrConstant));
  nt_value_decref(vcode);
  return exc;
}

ntValue *
nt_throw_exception_errno(const ntValue *ctx, int errorno)
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

  return nt_throw_exception_code(ctx, base, name, errorno, strerror(errorno));
}

ntValue *
nt_ensure_arguments(ntValue *arg, const char *fmt)
{
  char types[4096];
  unsigned int len = nt_value_as_long(nt_value_get_utf8(arg, "length"));
  unsigned int minimum = 0;
  unsigned int i, j;
  for (i = 0, j = 0; i < len; i++) {
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
        correct = nt_value_get_type(thsArg) == ntValueTypeBoolean;
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
        nt_value_decref(thsArg);
        return nt_throw_exception(arg, NULL, "SyntaxError", "Invalid format character!");
      }
    } while (!correct && depth > 0);
    nt_value_decref(thsArg);

    if (strlen(types) > 2)
      types[strlen(types) - 2] = '\0';

    if (strcmp(types, "") && !correct) {
      char msg[5120];
      snprintf(msg, 5120, "argument %u must be one of these types: %s", i, types);
      return nt_throw_exception(arg, NULL, "TypeError", msg);
    }
  }

  if (len < minimum) {
    return nt_throw_exception(arg, NULL, "TypeError", "Function requires at least %u arguments!", minimum);
  }

  return nt_value_new_undefined(arg);
}

ntValue *
nt_convert_arguments(ntValue *arg, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  ntValue *ret = nt_convert_arguments_varg (arg, fmt, ap);
  va_end(ap);
  return ret;
}

ntValue *
nt_convert_arguments_varg(ntValue *arg, const char *fmt, va_list ap)
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
    ntValue *val = nt_value_get_index(arg, i++);

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
        if (nt_value_is_string(val)) {
          p = va_arg(apc, void*);
          d = va_arg(apc, void*);
          size_t len = 0;
          ntChar *tmp = nt_value_to_string_utf16(val, &len);
          *((ntChar*) p) = len > 0 ? tmp[0] : (ntChar) 0;
          free(tmp);
          tmp = va_arg(apc, void*); // consume the default
        } else
          SET_ARGUMENT(ntChar);
        break;
      case 's':
        p = va_arg(apc, void*);
        d = va_arg(apc, void*);
        if (nt_value_is_undefined (val)) {
          if (d != NULL) {
            ssize_t len = 0;
            while (((ntChar*) d)[len++])
              ;
            ntChar *tmp = calloc (len+1, sizeof(ntChar));
            if (!tmp)
              goto nomem;
            for (; len >= 0; len--)
              tmp[len] = ((ntChar*) d)[len];
            d = tmp;
          }
          *((ntChar**) p) = (ntChar*) d;
        } else
          *((ntChar**) p) = (ntChar*) nt_value_to_string_utf16 (val, NULL);
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
      if (nt_value_is_string (val)) {
        p = va_arg(apc, void*);
        d = va_arg(apc, void*);

        size_t len = 0;
        char *tmp = nt_value_to_string_utf8 (val, &len);
        *((char*) p) = (tmp && len > 0) ? tmp[0] : (char) 0;
        free (tmp);
      } else
        SET_ARGUMENT(char);
      break;
    case 's':
      p = va_arg(apc, void*);
      d = va_arg(apc, void*);
      if (nt_value_is_undefined (val))
        *((char**) p) = strdup ((char*) d);
      else
        *((char**) p) = (char*) nt_value_to_string_utf8 (val, NULL);
      break;
    case '[':
      p = va_arg(apc, void*);
      d = va_arg(apc, void*);
      if (!strchr (++c, ']'))
        goto inval;
      if (!nt_value_is_undefined (val)) {
        char *tmp = strdup (c);
        if (!tmp)
          goto nomem;
        *strchr (tmp, ']') = '\0';
        *((void**) p) = nt_value_get_private_name (val, tmp);
        free (tmp);
      } else
        *((void**) p) = d;
      c = strchr (c, ']');
      break;
    case 'n':
      break;
    default:
      goto inval;
    }

    nt_value_decref(val);
    continue;

  inval:
    va_end(ap);
    nt_value_decref(val);
    return nt_throw_exception(arg, NULL, "SyntaxError", "Invalid format string!");

  nomem:
    va_end(ap);
    nt_value_decref(val);
    return NULL;
  }

  va_end(apc);
  return nt_value_new_number(arg, i);
}

ntValue *
nt_from_json(const ntValue *json)
{
  ntValue *glbl = nt_value_get_global(json);
  ntValue *JSON = nt_value_get_utf8(glbl, "JSON");
  ntValue *args = nt_value_new_array(glbl, json, NULL);
  ntValue *rslt = nt_value_call_utf8(JSON, "parse", args);

  nt_value_decref(args);
  nt_value_decref(JSON);
  return rslt;
}

ntValue *
nt_from_json_utf8(const ntValue *ctx, const char *json, size_t len)
{
  ntValue *JSON = nt_value_new_string_utf8_length(ctx, json, len);
  ntValue *rslt = nt_from_json(JSON);
  nt_value_decref(JSON);
  return rslt;
}

ntValue *
nt_from_json_utf16(const ntValue *ctx, const ntChar *json, size_t len)
{
  ntValue *JSON = nt_value_new_string_utf16_length(ctx, json, len);
  ntValue *rslt = nt_from_json(JSON);
  nt_value_decref(JSON);
  return rslt;
}

ntValue *
nt_to_json(const ntValue *val)
{
  ntValue *glbl = nt_value_get_global(val);
  ntValue *JSON = nt_value_get_utf8(glbl, "JSON");
  ntValue *args = nt_value_new_array(glbl, val, NULL);
  ntValue *rslt = nt_value_call_utf8(JSON, "stringify", args);

  nt_value_decref(args);
  nt_value_decref(JSON);
  return rslt;
}

char *
nt_to_json_utf8(const ntValue *val, size_t *len)
{
  ntValue *rslt = nt_to_json(val);
  char *srslt = nt_value_to_string_utf8(rslt, len);
  nt_value_decref(rslt);
  return srslt;
}

ntChar *
nt_to_json_utf16(const ntValue *val, size_t *len)
{
  ntValue *rslt = nt_to_json(val);
  ntChar *srslt = nt_value_to_string_utf16(rslt, len);
  nt_value_decref(rslt);
  return srslt;
}
