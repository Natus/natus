#ifndef NATUS_MODULE_H_
#define NATUS_MODULE_H_
#include <stdarg.h>

#include "natus.h"

#define NT_MODULE_ nt_module__
#define NT_MODULE(symb, init, free) \
	struct _ntModule NT_MODULE_ = { symb, init, free }
#define NT_MODULE_VERSION 1

struct _ntValue {
	unsigned int     vers;
	void            *priv;
	ntValue         *glbl;

	// Get a ntValue's type or value
	ntType         (*type)           (const ntValue *self);
	bool           (*as_bool)        (const ntValue *self);
	double         (*as_double)      (const ntValue *self);
	int            (*as_int)         (const ntValue *self);
	const char    *(*as_string)      (const ntValue *self);

	// Evaluate some javascript in the context of the current object
	const ntValue *(*evaluate)       (ntValue *self, const char *jscript, const char *filename, int lineno);

	// Functions for managing object properties
	bool           (*property_del)   (ntValue *self, const char *key);
	const ntValue *(*property_get)   (ntValue *self, const char *key);
	bool           (*property_set)   (ntValue *self, const char *key, va_list args);

	// Functions for getting/setting a private pointer
	void          *(*private_get)    (ntValue *self);
	bool           (*private_set)    (ntValue *self, void *priv);

	// Functions for handling arrays
	bool           (*array_del)      (ntValue *self, int index);
	const ntValue *(*array_get)      (ntValue *self, int index);
	bool           (*array_set)      (ntValue *self, int index, va_list args);
	int            (*array_len)      (ntValue *self);

	// Utility functions for inside of an ntJSFunction
	void           (*function_return)(ntValue *ret, va_list args);
};

struct _ntModule {
	const char  *symb;
	ntValue   *(*init)();
	void       (*free)(ntValue *global);
};

#define nt_get_arg_type(args) va_arg(args, ntType)

#endif /* NATUS_MODULE_H_ */
