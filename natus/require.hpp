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

#ifndef MODULE_HPP_
#define MODULE_HPP_
#include "types.hpp"
#include "value.hpp"
#include "misc.hpp"

#define NATUS_MODULE_INIT natus_module_init
#define NATUS_CHECK_ORIGIN(ctx, uri) \
	if (!Require(ctx).originPermitted(uri)) \
		return throwException(ctx, "SecurityError", "Permission denied!");

namespace natus {

class Require {
public:
	typedef enum {
		HookStepResolve,
		HookStepLoad,
		HookStepProcess
	} HookStep;

	typedef Value (*Hook)(Value& ctx, HookStep step, char* name, void* misc);
	typedef bool  (*OriginMatcher)(const char* pattern, const char* subject);
	typedef bool  (*ModuleInit)(ntValue* module);

	Require(Value ctx);
	bool initialize(Value config);
	bool initialize(const char* config);
	Value getConfig();

	bool addHook(const char* name, Hook func, void* misc=NULL, FreeFunction free=NULL);
	bool delHook(const char* name);

	bool addOriginMatcher(const char* name, OriginMatcher func, void* misc=NULL, FreeFunction free=NULL);
	bool delOriginMatcher(const char* name);

	Value require(const char* name);
	bool  originPermitted(const char* name);

private:
	Value ctx;
};

}
#endif /* MODULE_HPP_ */

