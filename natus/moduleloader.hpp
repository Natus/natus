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

#ifndef MODULELOADER_HPP_
#define MODULELOADER_HPP_
#include "types.hpp"

namespace natus {

typedef Value (*RequireFunction)(Value& module, const char* name, const char* reldir, const char** path, void* misc);
typedef bool  (*OriginMatcher)  (const char* pattern, const char* subject);

class ModuleLoader {
public:
	static ModuleLoader* getModuleLoader(const Value& ctx);
	static Value         getConfig(const Value& ctx);
	static Value         checkOrigin(const Value& ctx, string uri);

	ModuleLoader(Value& ctx);
	virtual ~ModuleLoader();
	Value initialize(string config="{}");
	int   addRequireHook(bool post, RequireFunction func, void* misc=NULL, FreeFunction free=NULL);
	void  delRequireHook(int id);
	int   addOriginMatcher(OriginMatcher func, void* misc=NULL, FreeFunction free=NULL);
	void  delOriginMatcher(int id);
	Value require(string name, string reldir, vector<string> path) const;
	bool  originPermitted(string uri) const;

private:
	void *internal;
};

}
#endif /* MODULELOADER_HPP_ */

