#include "test.hpp"

static Value function(Value& fnc, Value& ths, Value& arg) {
	return fnc.newBool(true);
}

int doTest(Engine& engine, Value& global) {
	Value array     = global.newArray();
	Value boolean   = global.newBool(true);
	Value func      = global.newFunction(function);
	Value null      = global.newNull();
	Value number    = global.newNumber(17);
	Value object    = global.newObject();
	Value string    = global.newString("hi");
	Value undefined = global.newUndefined();
	Value nullv     = NULL;

	assert( array.isArray());
	assert(!array.isBool());
	assert(!array.isFunction());
	assert(!array.isNull());
	assert(!array.isNumber());
	assert(!array.isObject());
	assert(!array.isString());
	assert(!array.isUndefined());
	assert(!array.isGlobal());
	assert(!array.isException());
	assert( array.toException().isException());

	assert(!boolean.isArray());
	assert( boolean.isBool());
	assert(!boolean.isFunction());
	assert(!boolean.isNull());
	assert(!boolean.isNumber());
	assert(!boolean.isObject());
	assert(!boolean.isString());
	assert(!boolean.isUndefined());
	assert(!boolean.isGlobal());
	assert(!boolean.isException());
	assert( boolean.toException().isException());

	assert(!func.isArray());
	assert(!func.isBool());
	assert( func.isFunction());
	assert(!func.isNull());
	assert(!func.isNumber());
	assert(!func.isObject());
	assert(!func.isString());
	assert(!func.isUndefined());
	assert(!func.isGlobal());
	assert(!func.isException());
	assert( func.toException().isException());

	assert(!null.isArray());
	assert(!null.isBool());
	assert(!null.isFunction());
	assert( null.isNull());
	assert(!null.isNumber());
	assert(!null.isObject());
	assert(!null.isString());
	assert(!null.isUndefined());
	assert(!null.isGlobal());
	assert(!null.isException());
	assert( null.toException().isException());

	assert(!number.isArray());
	assert(!number.isBool());
	assert(!number.isFunction());
	assert(!number.isNull());
	assert( number.isNumber());
	assert(!number.isObject());
	assert(!number.isString());
	assert(!number.isUndefined());
	assert(!number.isGlobal());
	assert(!number.isException());
	assert( number.toException().isException());

	assert(!object.isArray());
	assert(!object.isBool());
	assert(!object.isFunction());
	assert(!object.isNull());
	assert(!object.isNumber());
	assert( object.isObject());
	assert(!object.isString());
	assert(!object.isUndefined());
	assert(!object.isGlobal());
	assert(!object.isException());
	assert( object.toException().isException());

	assert(!string.isArray());
	assert(!string.isBool());
	assert(!string.isFunction());
	assert(!string.isNull());
	assert(!string.isNumber());
	assert(!string.isObject());
	assert( string.isString());
	assert(!string.isUndefined());
	assert(!string.isGlobal());
	assert(!string.isException());
	assert( string.toException().isException());

	assert(!undefined.isArray());
	assert(!undefined.isBool());
	assert(!undefined.isFunction());
	assert(!undefined.isNull());
	assert(!undefined.isNumber());
	assert(!undefined.isObject());
	assert(!undefined.isString());
	assert( undefined.isUndefined());
	assert(!undefined.isGlobal());
	assert(!undefined.isException());
	assert( undefined.toException().isException());

	assert(!nullv.isArray());
	assert(!nullv.isBool());
	assert(!nullv.isFunction());
	assert(!nullv.isNull());
	assert(!nullv.isNumber());
	assert(!nullv.isObject());
	assert(!nullv.isString());
	assert( nullv.isUndefined());
	assert(!nullv.isGlobal());
	assert( nullv.isException());

	return 0;
}
