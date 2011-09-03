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

#ifndef NATUS_HPP_
#define NATUS_HPP_

#ifndef I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#error Natus is not stable, go look elsewhere...
#endif

#include <string>
#include <cstdarg>
#include <stdint.h>

#undef NATUS_CHECK_ARGUMENTS
#define NATUS_CHECK_ARGUMENTS(arg, fmt) { \
  Value _exc = ensureArguments(arg, fmt); \
  if (_exc.isException()) return _exc; }

typedef struct natusValue natusValue;

namespace natus
{
class Value;

#ifdef WIN32
typedef wchar_t Char;
#else
typedef uint16_t Char;
#endif

#ifndef NULL
#ifdef __cplusplus
#define NULL 0
#else
#define NULL ((void *)0)
#endif
#endif

/* Type: FreeFunction
 * Function type for calls made back to free a memory value allocated outside natus.
 *
 * Parameters:
 *     mem - The memory to free.
 */
typedef void (*FreeFunction) (void *mem);

/* Type: NativeFunction
   * Function type for calls made back into native space from javascript.
   *
   * Parameters:
   *     fnc - The object representing the function itself.
   *     ths - The 'this' variable for the current execution.
   *           ths has a value of 'Undefined' when being run as a constructor.
   *     arg - The argument vector.
   *
   * Returns:
   *     The return value, which may be an exception.
   */
  typedef Value
  (*NativeFunction)(Value& fnc, Value& ths, Value& arg);

  typedef std::basic_string<char> UTF8;
  typedef std::basic_string<Char> UTF16;

  class Class {
  public:
    typedef enum {
      HookNone = 0,
      HookDel = 1 << 0,
      HookGet = 1 << 1,
      HookSet = 1 << 2,
      HookEnumerate = 1 << 3,
      HookCall = 1 << 4,
      HookProperties = (HookDel | HookGet | HookSet | HookEnumerate),
      HookAll = (HookProperties | HookCall),
    } Hooks;

    virtual Hooks
    getHooks();

    virtual Value
    del(Value& obj, Value& idx);

    virtual Value
    get(Value& obj, Value& idx);

    virtual Value
    set(Value& obj, Value& idx, Value& val);

    virtual Value
    enumerate(Value& obj);

    virtual Value
    call(Value& obj, Value& ths, Value& arg);

    virtual void
    free()
    {
      delete this;
    }
  };

  class Value {
  public:
    typedef enum {
      TypeUnknown = 0,
      TypeArray = 1 << 0,
      TypeBool = 1 << 1,
      TypeFunction = 1 << 2,
      TypeNull = 1 << 3,
      TypeNumber = 1 << 4,
      TypeObject = 1 << 5,
      TypeString = 1 << 6,
      TypeUndefined = 1 << 7,
      TypeSupportsPrivate = TypeFunction | TypeObject,
    } Type;

    typedef enum {
      PropAttrNone = 0,
      PropAttrReadOnly = 1 << 0,
      PropAttrDontEnumerate = 1 << 1,
      PropAttrDontDelete = 1 << 2,
      PropAttrConstant = PropAttrReadOnly | PropAttrDontDelete,
      PropAttrProtected = PropAttrConstant | PropAttrDontEnumerate
    } PropAttr;

    static Value
    newGlobal(const Value global);

    static Value
    newGlobal(const char *name_or_path = NULL);

    Value();

    Value(natusValue *value, bool steal = true);

    Value(const Value& value);

    ~Value();

    Value&
    operator=(const Value& value);

    bool
    operator==(const Value& value) const;

    bool
    operator==(bool value) const;

    bool
    operator==(int value) const;

    bool
    operator==(long value) const;

    bool
    operator==(double value) const;

    bool
    operator==(const char* value) const;

    bool
    operator==(const Char* value) const;

    bool
    operator==(UTF8 value) const;

    bool
    operator==(UTF16 value) const;

    bool
    operator!=(const Value& value) const;

    bool
    operator!=(bool value) const;

    bool
    operator!=(int value) const;

    bool
    operator!=(long value) const;

    bool
    operator!=(double value) const;

    bool
    operator!=(const char* value) const;

    bool
    operator!=(const Char* value) const;

    bool
    operator!=(UTF8 value) const;

    bool
    operator!=(UTF16 value) const;

    Value
    operator[](long index);

    Value
    operator[](Value& index);

    Value
    operator[](UTF8 string);

    Value
    operator[](UTF16 string);

    Value
    newBoolean(bool b) const;

    Value
    newNumber(double n) const;

    Value
    newString(UTF8 string) const;

    Value
    newString(UTF16 string) const;

    Value
    newString(const char* fmt, va_list arg);

    Value
    newString(const char* fmt, ...);

    Value
    newArray(const Value* const * array = NULL) const;

    Value
    newArray(va_list ap) const;

    Value
    newArray(const Value* item, ...) const;

    Value
    newFunction(NativeFunction func, const char* name = NULL) const;

    Value
    newObject(Class* cls = NULL) const;

    Value
    newNull() const;

    Value
    newUndefined() const;

    Value
    getGlobal() const;

    const char*
    getEngineName() const;

    Value::Type
    getType() const;

    const char*
    getTypeName() const;

    bool
    borrowContext(void **context, void **value) const;

    natusValue*
    borrowCValue() const;

    bool
    isException() const;

    bool
    isType(Value::Type types) const;

    bool
    isArray() const;

    bool
    isBoolean() const;

    bool
    isFunction() const;

    bool
    isNull() const;

    bool
    isNumber() const;

    bool
    isObject() const;

    bool
    isString() const;

    bool
    isUndefined() const;

    Value
    toException();
    template<class T>
      T
      to() const
      {
        return (T) to<double>();
      }

    Value
    del(Value idx);

    Value
    del(UTF8 idx);

    Value
    del(UTF16 idx);

    Value
    del(size_t idx);

    Value
    delRecursive(UTF8 path);

    Value
    get(Value idx) const;

    Value
    get(UTF8 idx) const;

    Value
    get(UTF16 idx) const;

    Value
    get(size_t idx) const;

    Value
    getRecursive(UTF8 path);

    Value
    set(Value idx, Value value, Value::PropAttr attrs = PropAttrNone);

    Value
    set(Value idx, bool value, Value::PropAttr attrs = PropAttrNone);

    Value
    set(Value idx, int value, Value::PropAttr attrs = PropAttrNone);

    Value
    set(Value idx, long value, Value::PropAttr attrs = PropAttrNone);

    Value
    set(Value idx, double value, Value::PropAttr attrs = PropAttrNone);

    Value
    set(Value idx, const char* value, Value::PropAttr attrs = PropAttrNone);

    Value
    set(Value idx, const Char* value, Value::PropAttr attrs = PropAttrNone);

    Value
    set(Value idx, UTF8 value, Value::PropAttr attrs = PropAttrNone);

    Value
    set(Value idx, UTF16 value, Value::PropAttr attrs = PropAttrNone);

    Value
    set(Value idx, NativeFunction value, Value::PropAttr attrs = PropAttrNone);

    Value
    set(Value idx, NativeFunction value, const char *name, Value::PropAttr attrs = PropAttrNone);

    Value
    set(UTF8 idx, Value value, Value::PropAttr attrs = PropAttrNone);

    Value
    set(UTF8 idx, bool value, Value::PropAttr attrs = PropAttrNone);

    Value
    set(UTF8 idx, int value, Value::PropAttr attrs = PropAttrNone);

    Value
    set(UTF8 idx, long value, Value::PropAttr attrs = PropAttrNone);

    Value
    set(UTF8 idx, double value, Value::PropAttr attrs = PropAttrNone);

    Value
    set(UTF8 idx, const char* value, Value::PropAttr attrs = PropAttrNone);

    Value
    set(UTF8 idx, const Char* value, Value::PropAttr attrs = PropAttrNone);

    Value
    set(UTF8 idx, UTF8 value, Value::PropAttr attrs = PropAttrNone);

    Value
    set(UTF8 idx, UTF16 value, Value::PropAttr attrs = PropAttrNone);

    Value
    set(UTF8 idx, NativeFunction value, Value::PropAttr attrs = PropAttrNone);

    Value
    set(UTF8 idx, NativeFunction value, const char *name, Value::PropAttr attrs = PropAttrNone);

    Value
    set(UTF16 idx, Value value, Value::PropAttr attrs = PropAttrNone);

    Value
    set(UTF16 idx, bool value, Value::PropAttr attrs = PropAttrNone);

    Value
    set(UTF16 idx, int value, Value::PropAttr attrs = PropAttrNone);

    Value
    set(UTF16 idx, long value, Value::PropAttr attrs = PropAttrNone);

    Value
    set(UTF16 idx, double value, Value::PropAttr attrs = PropAttrNone);

    Value
    set(UTF16 idx, const char* value, Value::PropAttr attrs = PropAttrNone);

    Value
    set(UTF16 idx, const Char* value, Value::PropAttr attrs = PropAttrNone);

    Value
    set(UTF16 idx, UTF8 value, Value::PropAttr attrs = PropAttrNone);

    Value
    set(UTF16 idx, UTF16 value, Value::PropAttr attrs = PropAttrNone);

    Value
    set(UTF16 idx, NativeFunction value, Value::PropAttr attrs = PropAttrNone);

    Value
    set(UTF16 idx, NativeFunction value, const char *name, Value::PropAttr attrs = PropAttrNone);

    Value
    set(size_t idx, Value value);

    Value
    set(size_t idx, bool value);

    Value
    set(size_t idx, int value);

    Value
    set(size_t idx, long value);

    Value
    set(size_t idx, double value);

    Value
    set(size_t idx, const char* value);

    Value
    set(size_t idx, const Char* value);

    Value
    set(size_t idx, UTF8 value);

    Value
    set(size_t idx, UTF16 value);

    Value
    set(size_t idx, NativeFunction value);

    Value
    set(size_t idx, NativeFunction value, const char *name);

    Value
    setRecursive(UTF8 path, Value val, Value::PropAttr attrs = PropAttrNone, bool mkpath = false);

    Value
    setRecursive(UTF8 path, bool val, Value::PropAttr attrs = PropAttrNone, bool mkpath = false);

    Value
    setRecursive(UTF8 path, int val, Value::PropAttr attrs = PropAttrNone, bool mkpath = false);

    Value
    setRecursive(UTF8 path, long val, Value::PropAttr attrs = PropAttrNone, bool mkpath = false);

    Value
    setRecursive(UTF8 path, double val, Value::PropAttr attrs = PropAttrNone, bool mkpath = false);

    Value
    setRecursive(UTF8 path, const char* val, Value::PropAttr attrs = PropAttrNone, bool mkpath = false);

    Value
    setRecursive(UTF8 path, const Char* val, Value::PropAttr attrs = PropAttrNone, bool mkpath = false);

    Value
    setRecursive(UTF8 path, UTF8 val, Value::PropAttr attrs = PropAttrNone, bool mkpath = false);

    Value
    setRecursive(UTF8 path, UTF16 val, Value::PropAttr attrs = PropAttrNone, bool mkpath = false);

    Value
    setRecursive(UTF8 path, NativeFunction val, Value::PropAttr attrs = PropAttrNone, bool mkpath = false);

    Value
    setRecursive(UTF8 path, NativeFunction val, const char *name, Value::PropAttr attrs = PropAttrNone, bool mkpath = false);

    Value
    enumerate() const;

    Value
    push(Value item);

    Value
    push(bool item);

    Value
    push(int item);

    Value
    push(long item);

    Value
    push(double item);

    Value
    push(const char* item);

    Value
    push(const Char* item);

    Value
    push(UTF8 item);

    Value
    push(UTF16 item);

    Value
    push(NativeFunction item, const char *name=NULL);

    Value
    pop();

#define getPrivate(type) getPrivateName<type>(# type)
    template<class T>
      T
      getPrivateName(const char* key) const
      {
        return (T) getPrivateName<void*>(key);
      }
#define setPrivate(type, ...) setPrivateName<type>(# type, __VA_ARGS__)
    template<class T>
      bool
      setPrivateName(const char* key, T priv, FreeFunction free = NULL)
      {
        return setPrivateName<void*>(key, (void*) priv, free);
      }

    Value
    evaluate(Value javascript, Value filename, unsigned int lineno = 0);

    Value
    evaluate(UTF8 javascript, UTF8 filename = "", unsigned int lineno = 0);

    Value
    evaluate(UTF16 javascript, UTF16 filename, unsigned int lineno = 0);

    Value
    call(Value ths, va_list ap);

    Value
    call(Value ths, Value args = NULL);

    Value
    call(Value ths, const Value *arg0, ...);

    Value
    call(UTF8 name, va_list ap);

    Value
    call(UTF8 name, Value args = NULL);

    Value
    call(UTF8 name, const Value *arg0, ...);

    Value
    call(UTF16 name, va_list ap);

    Value
    call(UTF16 name, Value args = NULL);

    Value
    call(UTF16 name, const Value *arg0, ...);

    Value
    callNew(va_list ap);

    Value
    callNew(Value args = NULL);

    Value
    callNew(const Value *arg0, ...);

    Value
    callNew(UTF8 name, va_list ap);

    Value
    callNew(UTF8 name, Value args = NULL);

    Value
    callNew(UTF8 name, const Value *arg0, ...);

    Value
    callNew(UTF16 name, va_list ap);

    Value
    callNew(UTF16 name, Value args = NULL);

    Value
    callNew(UTF16 name, const Value *arg0, ...);

    bool
    equals(Value val) const;

    bool
    equals(bool value) const;

    bool
    equals(int value) const;

    bool
    equals(long value) const;

    bool
    equals(double value) const;

    bool
    equals(const char* value) const;

    bool
    equals(const Char* value) const;

    bool
    equals(UTF8 value) const;

    bool
    equals(UTF16 value) const;

    bool
    equalsStrict(Value val) const;

    bool
    equalsStrict(bool value) const;

    bool
    equalsStrict(int value) const;

    bool
    equalsStrict(long value) const;

    bool
    equalsStrict(double value) const;

    bool
    equalsStrict(const char* value) const;

    bool
    equalsStrict(const Char* value) const;

    bool
    equalsStrict(UTF8 value) const;

    bool
    equalsStrict(UTF16 value) const;

  private:
    natusValue *internal;
  };

  template<>
    bool
    Value::to<bool>() const;

  template<>
    double
    Value::to<double>() const;

  template<>
    int
    Value::to<int>() const;

  template<>
    long
    Value::to<long>() const;

  template<>
    UTF8
    Value::to<UTF8>() const;

  template<>
    UTF16
    Value::to<UTF16>() const;

  template<>
    void*
    Value::getPrivateName<void*>(const char* key) const;

  template<>
    Value
    Value::getPrivateName<Value>(const char* key) const;

  template<>
    bool
    Value::setPrivateName<void*>(const char* key, void* priv, FreeFunction free);

  template<>
    bool
    Value::setPrivateName<Value>(const char* key, Value priv, FreeFunction free);

  Value
  throwException(Value ctx, const char* base, const char* type, const char* format, va_list ap);

  Value
  throwException(Value ctx, const char* base, const char* type, const char* format, ...);

  Value
  throwException(Value ctx, const char* base, const char* type, int code, const char* format, va_list ap);

  Value
  throwException(Value ctx, const char* base, const char* type, int code, const char* format, ...);

  Value
  throwException(Value ctx, int errorno);

  Value
  ensureArguments(Value args, const char* fmt);

  Value
  convertArguments(Value args, const char *fmt, va_list ap);

  Value
  convertArguments(Value args, const char *fmt, ...);

  Value
  arrayBuilder(Value array, Value item);

  Value
  arrayBuilder(Value array, bool item);

  Value
  arrayBuilder(Value array, int item);

  Value
  arrayBuilder(Value array, long item);

  Value
  arrayBuilder(Value array, double item);

  Value
  arrayBuilder(Value array, const char* item);

  Value
  arrayBuilder(Value array, const Char* item);

  Value
  arrayBuilder(Value array, UTF8 item);

  Value
  arrayBuilder(Value array, UTF16 item);

  Value
  arrayBuilder(Value array, NativeFunction item);

} // namespace natus
#endif /* NATUS_HPP_ */

