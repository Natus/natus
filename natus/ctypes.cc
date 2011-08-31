#include <natus-internal.hh>

template<>
  bool
  Value::to<bool>() const
  {
    return natus_to_bool(internal);
  }

template<>
  double
  Value::to<double>() const
  {
    return natus_to_double(internal);
  }

template<>
  int
  Value::to<int>() const
  {
    return natus_to_int(internal);
  }

template<>
  long
  Value::to<long>() const
  {
    return natus_to_long(internal);
  }

template<>
  UTF8
  Value::to<UTF8>() const
  {
    size_t len;
    char* tmp = natus_to_string_utf8(internal, &len);
    if (!tmp)
      return UTF8();
    UTF8 rslt(tmp, len);
    free(tmp);
    return rslt;
  }

template<>
  UTF16
  Value::to<UTF16>() const
  {
    size_t len;
    Char* tmp = natus_to_string_utf16(internal, &len);
    if (!tmp)
      return UTF16();
    UTF16 rslt(tmp, len);
    free(tmp);
    return rslt;
  }
