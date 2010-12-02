#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include <natus/natus_module.h>

#include <jsapi.h>

typedef struct {
	ntValue ntv;
	JSRuntime *run;
	JSContext *ctx;
	jsval val;
} mozValue;

typedef struct {
	ntJSFunction func;
	void        *misc;
	mozValue    *glbl;
} mozFunctionPriv;

#define CTX(s)     (((mozValue *) s)->ctx)
#define VAL(s)     (((mozValue *) s)->val)
#define OVAL(s)    (JSVAL_TO_OBJECT(VAL(s)))
#define GLBV(s)    (s->glbl ? s->glbl : s)

#define ERROR(c, s) { JS_SetPendingException(c, STRING_TO_JSVAL(JS_NewStringCopyZ(c, s))); return JS_FALSE; }

static bool nt_value_from_jsval(mozValue *self, mozValue *proto, jsval val);

// Setup Javascript global class
// This MUST be a static global
static JSClass glb = { "global", JSCLASS_GLOBAL_FLAGS, JS_PropertyStub,
		JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_EnumerateStub,
		JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub, };

static void nfunc_finalize(JSContext *ctx, JSObject *obj) {
	free(JS_GetPrivate(ctx, obj));
	return JS_FinalizeStub(ctx, obj);
}

// Setup Javascript function pointer class
// This MUST be a static global
static JSClass nfunc = { "__native_func__", JSCLASS_HAS_PRIVATE, JS_PropertyStub,
		JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_EnumerateStub,
		JS_ResolveStub, JS_ConvertStub, nfunc_finalize, };

static void freevala(mozValue **a) {
	int i;
	for (i = 0; a && a[i]; i++)
		free(a[i]);
	free(a);
}

static JSBool on_function_call(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval) {
	int i;

	// Find out the native function pointer for this function
	jsval val;
	if (!JS_GetReservedSlot(cx, JSVAL_TO_OBJECT(JS_ARGV_CALLEE(argv)), 0, &val))
		ERROR(cx, "Unable to find native function info!");
	if (!JSVAL_IS_OBJECT(val))
		ERROR(cx, "Unable to find native function info!");
	ntJSFunction func = ((mozFunctionPriv *) JS_GetPrivate(cx, JSVAL_TO_OBJECT(val)))->func;
	mozValue    *glbl = ((mozFunctionPriv *) JS_GetPrivate(cx, JSVAL_TO_OBJECT(val)))->glbl;
	if (!func || !glbl)
		ERROR(cx, "Unable to find native function info!");

	// Allocate our array of arguments
	mozValue **arg = malloc(sizeof(mozValue *) * (argc + 1));
	if (arg)
		memset(arg, 0, sizeof(mozValue *) * (argc + 1));
	for (i = 0; arg && i < argc; i++) {
		arg[i] = malloc(sizeof(mozValue));
		if (!arg[i] || !nt_value_from_jsval(arg[i], glbl, argv[i])) {
			freevala(arg);
			JS_ReportOutOfMemory(cx);
			return JS_FALSE;
		}
	}

	// Allocate this and function objects
	mozValue  rvl;
	mozValue *ret = &rvl;
	mozValue  ths;
	mozValue  fnc;
	if (!arg ||
		!nt_value_from_jsval(&rvl, glbl, JSVAL_VOID) ||
		!nt_value_from_jsval(&ths, glbl, OBJECT_TO_JSVAL(obj)) ||
		!nt_value_from_jsval(&fnc, glbl, JS_ARGV_CALLEE(argv))) {
		if (arg)
			freevala(arg);
		JS_ReportOutOfMemory(cx);
		return JS_FALSE;
	}

	// Call our native function
	bool result = func((ntValue **)      &ret,
	                   (ntValue *)       glbl,
                       (ntValue *)       &ths,
                       (ntValue *)       &fnc,
                       (const ntValue **) arg,
			           ((mozFunctionPriv *) JS_GetPrivate(cx, JSVAL_TO_OBJECT(val)))->misc);
	freevala(arg);

	// Handle the results
	if (!result) {
		if (ret == NULL)
			JS_ReportOutOfMemory(cx);
		else
			JS_SetPendingException(cx, ret->val);
		return JS_FALSE;
	}

	*rval = ret->val;
	return JS_TRUE;
}

static void report_error(JSContext *cx, const char *message,
		JSErrorReport *report) {
	fprintf(stderr, "%s:%u:%s\n", report->filename ? report->filename
			: "<no filename>", (unsigned int) report->lineno, message);
}

static jsval jsvalget(JSContext *cx, JSObject *root, const char *nmspace) {
	jsval val;
	if (!cx)
		return JSVAL_VOID;
	if (!root)
		root = JS_GetGlobalObject(cx);

	if (strchr(nmspace, '.') == NULL) {
		if (!JS_GetProperty(cx, root, nmspace, &val)) {
			if (!strcmp(nmspace, ""))
				return OBJECT_TO_JSVAL(root);
			return JSVAL_VOID;
		}
		return val;
	}

	char *thisname = strndup(nmspace, strchr(nmspace, '.') - nmspace);
	if (!thisname)
		return JSVAL_VOID;
	char *chldname = strdup(strchr(nmspace, '.') + 1);
	if (!chldname) {
		free(thisname);
		return JSVAL_VOID;
	}

	if (!JS_GetProperty(cx, root, thisname, &val) || !JSVAL_IS_OBJECT(val)) {
		free(thisname);
		free(chldname);
		return JSVAL_VOID;
	}

	root = JSVAL_TO_OBJECT(val);
	free(thisname);
	if (!root) {
		free(chldname);
		return JSVAL_VOID;
	}

	val = jsvalget(cx, root, chldname);
	free(chldname);
	return val;
}

static JSObject *mkfunc(mozValue *global, ntJSFunction func, void *msc) {
	JSFunction *fnc = JS_NewFunction(CTX(global), on_function_call, 8, 0, NULL, NULL);
	JSObject *obj = JS_GetFunctionObject(fnc);
	JSObject *prv = JS_NewObject(CTX(global), &nfunc, NULL, NULL);
	if (!fnc || !obj || !prv)
		return NULL;

	mozFunctionPriv *private = malloc(sizeof(mozFunctionPriv));
	if (!private)
		return NULL;

	private->func = func;
	private->misc = msc;
	private->glbl = global;
	if (!JS_SetPrivate(CTX(global), prv, private)) {
		free(private);
		return NULL;
	}

	if (!JS_SetReservedSlot(CTX(global), obj, 0, OBJECT_TO_JSVAL(prv)))
		return NULL;
	return obj;
}

static jsval jsval_from_arg(mozValue *glbl, va_list args) {
	double d;
	int len, i;
	jsval *array;
	JSObject *obj;
	ntJSFunction func;
	void *misc;

	switch (nt_get_arg_type(args)) {
		case NT_TYPE_ARRAY:
			len = va_arg(args, int);
			array = malloc(sizeof(jsval *) * (len+1));
			if (!array) return JSVAL_VOID;
			for (i=0 ; i < len ; i++)
				array[i] = jsval_from_arg(glbl, args);
			obj = JS_NewArrayObject(CTX(glbl), len, array);
			free(array);
			if (!obj)
				return JSVAL_VOID;
			return OBJECT_TO_JSVAL(obj);
		case NT_TYPE_BOOL:
			return BOOLEAN_TO_JSVAL(va_arg(args, int));
		case NT_TYPE_DOUBLE:
			d = va_arg(args, double);
			return DOUBLE_TO_JSVAL(&d);
		case NT_TYPE_FUNCTION:
			func = va_arg(args, ntJSFunction);
			misc = va_arg(args, void *);
			return OBJECT_TO_JSVAL(mkfunc(glbl, func, misc));
		case NT_TYPE_INTEGER:
			return INT_TO_JSVAL(va_arg(args, int));
		case NT_TYPE_NULL:
			return JSVAL_NULL;
		case NT_TYPE_OBJECT:
			return OBJECT_TO_JSVAL(JS_NewObject(CTX(glbl), NULL, NULL, NULL));
		case NT_TYPE_STRING:
			return STRING_TO_JSVAL(JS_NewStringCopyZ(CTX(glbl), va_arg(args, const char *)));
		case NT_TYPE_VALUE:
			return VAL(va_arg(args, mozValue *));
		case NT_TYPE_UNDEFINED:
		default:
			return JSVAL_VOID;
	}
}

static ntType type(const ntValue *self) {
	assert(self);
	if (JSVAL_IS_BOOLEAN(VAL(self)))
		return NT_TYPE_BOOL;
	else if (JSVAL_IS_DOUBLE(VAL(self)))
		return NT_TYPE_DOUBLE;
	else if (JSVAL_IS_INT(VAL(self)))
		return NT_TYPE_INTEGER;
	else if (JSVAL_IS_NULL(VAL(self)))
		return NT_TYPE_NULL;
	else if (JSVAL_IS_STRING(VAL(self)))
		return NT_TYPE_STRING;
	else if (JSVAL_IS_OBJECT(VAL(self))) {
		if (JS_ObjectIsFunction(CTX(self), OVAL(self)))
			return NT_TYPE_FUNCTION;
		else if (JS_IsArrayObject(CTX(self), OVAL(self)))
			return NT_TYPE_ARRAY;
		return NT_TYPE_OBJECT;
	}

	return NT_TYPE_UNDEFINED;
}

static bool as_bool(const ntValue *self) {
	assert(self);
	return JSVAL_TO_BOOLEAN(VAL(self));
}

static double as_double(const ntValue *self) {
	assert(self);
	return *JSVAL_TO_DOUBLE(VAL(self));
}

static int as_int(const ntValue *self) {
	assert(self);
	return JSVAL_TO_INT(VAL(self));
}

static const char *as_string(const ntValue *self) {
	assert(self);
	return JS_GetStringBytes(JSVAL_TO_STRING(VAL(self)));
}

static const ntValue *evaluate(ntValue *self, const char *jscript, const char *filename, int lineno) {
	static mozValue retval;
	assert(self);
	assert(jscript);
	assert(type(self) == NT_TYPE_OBJECT || type(self) == NT_TYPE_FUNCTION);

	jsval rval;
	if (JS_EvaluateScript(CTX(self), OVAL(self), jscript,
			strlen(jscript), filename, lineno, &rval) == JS_FALSE)
		return NULL;
	if (!nt_value_from_jsval(&retval, (mozValue *) GLBV(self), rval))
		return NULL;
	return (ntValue *) &retval;
}

static bool property_del(ntValue *self, const char *key) {
	assert(self);
	if (!JSVAL_IS_OBJECT(VAL(self)))
		return false;

	JSObject *obj = NULL;

	char *tmp = strrchr(key, '.');
	if (tmp) {
		char *tmpa = strndup(key, --tmp - key);
		if (!tmpa)
			return false;
		jsval val = jsvalget(CTX(self), OVAL(self), tmpa);
		free(tmpa);
		if (!JSVAL_IS_OBJECT(val))
			return false;
		obj = JSVAL_TO_OBJECT(val);
	} else
		obj = OVAL(self);

	return JS_DeleteProperty(CTX(self), obj, tmp ? ++tmp : key);
}

static const ntValue *property_get(ntValue *self, const char *key) {
	static mozValue retval;
	assert(self);
	if (!JSVAL_IS_OBJECT(VAL(self)))
		return NULL;

	jsval val = jsvalget(CTX(self), OVAL(self), key);
	if (JSVAL_IS_VOID(val))
		return NULL;
	if (nt_value_from_jsval(&retval, (mozValue *) GLBV(self), val))
		return (ntValue *) &retval;
	return NULL;
}

static bool nmspace_split(char **base, char **name) {
	char *tmp = strrchr(*base, '.');
	*name = tmp ? tmp + 1 : *base;
	*base = tmp ? strndup(*base, tmp-*base) : NULL;

	return (*base || !tmp);
}

static bool property_set(ntValue *self, const char *key, va_list args) {
	assert(self);
	if (!JSVAL_IS_OBJECT(VAL(self)))
		return false;

	char *name, *base = (char *) key;
	if (!nmspace_split(&base, &name))
		return false;

	jsval bval = VAL(self);
	if (base) {
		bval = jsvalget(CTX(self), OVAL(self), base);
		free(base);
		if (!JSVAL_IS_OBJECT(bval))
			return false;
	}

	jsval v = jsval_from_arg((mozValue *) GLBV(self), args);
	return JS_SetProperty(CTX(self), JSVAL_TO_OBJECT(bval), name, &v);
}

static void *private_get(ntValue *self) {
	assert(self);
	if (JSVAL_IS_PRIMITIVE(VAL(self)))
		return NULL;
	return JS_GetPrivate(CTX(self), OVAL(self));
}

static bool private_set(ntValue *self, void *priv) {
	assert(self);
	if (JSVAL_IS_PRIMITIVE(VAL(self)))
		return false;
	return JS_SetPrivate(CTX(self), OVAL(self), priv);
}

static bool array_del(ntValue *self, int index) {
	if (type(self) != NT_TYPE_ARRAY) return false;
	return JS_DeleteElement(CTX(self), OVAL(self), index);
}

static const ntValue *array_get(ntValue *self, int index) {
	static mozValue retval;
	if (type(self) != NT_TYPE_ARRAY) return NULL;

	jsval val;
	if (!JS_GetElement(CTX(self), OVAL(self), index, &val)) return NULL;
	if (!nt_value_from_jsval(&retval, (mozValue *) GLBV(self), val))
		return NULL;
	return (ntValue *) &retval;
}

static bool array_set(ntValue *self, int index, va_list args) {
	if (type(self) != NT_TYPE_ARRAY) return false;

	jsval val = jsval_from_arg((mozValue *) GLBV(self), args);
	return JS_SetElement(CTX(self), OVAL(self), index, &val);
}

static int  array_len(ntValue *self) {
	if (type(self) != NT_TYPE_ARRAY) return -1;

	int len;
	if (!JS_GetArrayLength(CTX(self), OVAL(self), &len)) return -1;
	return len;
}

static void function_return(ntValue *ret, va_list args) {
	nt_value_from_jsval((mozValue *) ret,
			            (mozValue *) GLBV(ret),
			            jsval_from_arg((mozValue *) GLBV(ret), args));
}

static void mozjs_free(ntValue *self) {
	if (!self)
		return;
	if (CTX(self))
		JS_DestroyContext(CTX(self));
	if (RUN(self))
		JS_DestroyRuntime(((mozValue *) self)->run);
	JS_ShutDown();
}

static bool nt_value_from_jsval(mozValue *self, mozValue *proto, jsval val) {
	if (!self) return false;
	memset(self, 0, sizeof(mozValue));

	if (proto) {
		memcpy(self, proto, sizeof(mozValue));
		if (!self->ntv.glbl)
			self->ntv.glbl = (ntValue *) proto;
		self->val = val;
		return true;
	}

	self->ntv.vers = NT_MODULE_VERSION;
	self->ntv.glbl = NULL;
	self->run = NULL;
	self->ctx = NULL;

	self->ntv.type = type;
	self->ntv.as_bool = as_bool;
	self->ntv.as_double = as_double;
	self->ntv.as_int = as_int;
	self->ntv.as_string = as_string;
	self->ntv.evaluate = evaluate;
	self->ntv.property_del = property_del;
	self->ntv.property_get = property_get;
	self->ntv.property_set = property_set;
	self->ntv.private_get = private_get;
	self->ntv.private_set = private_set;
	self->ntv.array_del = array_del;
	self->ntv.array_get = array_get;
	self->ntv.array_set = array_set;
	self->ntv.array_len = array_len;
	self->ntv.function_return = function_return;

	// Initialize Javascript runtime environment
	JSObject *glbl;
	if (!(self->run = JS_NewRuntime(1024 * 1024)))            goto error;
	if (!(CTX(self) = JS_NewContext(self->run, 1024 * 1024))) goto error;
	JS_SetOptions(CTX(self), JSOPTION_VAROBJFIX);
	JS_SetVersion(CTX(self), JSVERSION_LATEST);
	JS_SetErrorReporter(CTX(self), report_error);
	if (!(glbl = JS_NewObject(CTX(self), &glb, NULL, NULL)))  goto error;
	if (!JS_InitStandardClasses(CTX(self), glbl))             goto error;
	self->val = OBJECT_TO_JSVAL(glbl);
	return true;

	error:
		mozjs_free((ntValue *) self);
		return false;
}

static ntValue *mozjs_init() {
	mozValue *self = malloc(sizeof(mozValue));
	if (nt_value_from_jsval(self, NULL, JSVAL_NULL))
		return (ntValue *) self;
	free(self);
	return NULL;
}
NT_MODULE("JS_GetProperty", mozjs_init, mozjs_free);
