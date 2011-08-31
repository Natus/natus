#include <natus-internal.hh>

template<>
  void*
  Value::getPrivateName<void*>(const char* key) const
  {
    return natus_get_private_name(internal, key);
  }

template<>
  Value
  Value::getPrivateName<Value>(const char* key) const
  {
    return natus_get_private_name_value(internal, key);
  }

template<>
  bool
  Value::setPrivateName<void*>(const char* key, void* priv, FreeFunction free)
  {
    return natus_set_private_name(internal, key, priv, free);
  }

template<>
  bool
  Value::setPrivateName<Value>(const char* key, Value priv, FreeFunction free)
  {
    return natus_set_private_name_value(internal, key, priv.internal);
  }
