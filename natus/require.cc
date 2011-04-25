#include "require.h"
#include "require.hpp"
using namespace natus;

struct HookMisc {
	void*        misc;
	FreeFunction free;
	Require::Hook func;

	static void freeit(void* hm) {
		delete (HookMisc*) hm;
	}

	static ntValue* translate(ntValue* ctx, ntRequireHookStep step, char* name, void* misc) {
		if (!misc) return NULL;
		HookMisc* hm = (HookMisc*) misc;

		Value ctxt(ctx, false);
		Value rslt = hm->func(ctxt, (Require::HookStep) step, name, hm->misc);
		return nt_value_incref(rslt.borrowCValue());
	}

	HookMisc(Require::Hook fnc, void* m, FreeFunction f) {
		func = fnc;
		misc = m;
		free = f;
	}

	~HookMisc() {
		if (misc && free) free(misc);
	}
};

Require::Require(Value ctx) {
	this->ctx = ctx;
}

bool Require::initialize(Value config) {
	return nt_require_init_value(ctx.borrowCValue(), config.borrowCValue());
}

bool Require::initialize(const char* config) {
	return nt_require_init(ctx.borrowCValue(), config);
}

Value Require::getConfig() {
	return nt_require_get_config(ctx.borrowCValue());
}

bool Require::addHook(const char* key, Hook func, void* misc, FreeFunction free) {
	return nt_require_hook_add(ctx.borrowCValue(), key, HookMisc::translate, new HookMisc(func, misc, free), HookMisc::freeit);
}

bool Require::delHook(const char* key) {
	return nt_require_hook_del(ctx.borrowCValue(), key);
}

bool Require::addOriginMatcher(const char* key, OriginMatcher func, void* misc, FreeFunction free) {
	return nt_require_origin_matcher_add(ctx.borrowCValue(), key, func, misc, free);
}

bool Require::delOriginMatcher(const char* key) {
	return nt_require_origin_matcher_del(ctx.borrowCValue(), key);
}

Value Require::require(const char* name) {
	return nt_require(ctx.borrowCValue(), name);
}

bool Require::originPermitted(const char* uri) {
	return nt_require_origin_permitted(ctx.borrowCValue(), uri);
}

