#ifndef NATUS_ENGINE_H_
#define NATUS_ENGINE_H_
#include <stdarg.h>

#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#include "natus.h"
namespace natus {

#define NATUS_ENGINE_VERSION 1
#define NATUS_ENGINE_ natus_engine__
#define NATUS_ENGINE(name, symb, init, newg, free) \
	EngineSpec NATUS_ENGINE_ = { NATUS_ENGINE_VERSION, name, symb, init, newg, free }

class EngineValue : virtual public BaseValue<Value> {
public:
	EngineValue(EngineValue* glb, bool exception=false);
	void   incRef();
	void   decRef();
	Value  toException(Value& value);
	bool   isException();
	Value  getGlobal();
	virtual bool supportsPrivate()=0;

protected:
	unsigned long  refCnt;
	bool           exc;
	EngineValue*   glb;

	template <class T> static T* borrowInternal(Value& value) {
		return static_cast<T*>(value.internal);
	}
};

struct EngineSpec {
	unsigned int  vers;
	const char   *name;
	const char   *symb;
	void*       (*init)();
	EngineValue*(*newg)(void *);
	void        (*free)(void *);
};

struct ClassFuncPrivate {
	Class*         clss;
	NativeFunction func;
	void*          priv;
	FreeFunction   free;
	EngineValue*   glbl;

	~ClassFuncPrivate() {
		if (clss)
			delete clss;
		if (free)
			free(priv);
	}
};

}  // namespace natus
#endif /* NATUS_ENGINE_H_ */
