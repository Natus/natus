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

#include <cstdlib>
#include <cstdio>

#include "engine.hh"
#include "value.hh"
#include "engine.h"
using namespace natus;

Engine::Engine() {
	internal = NULL;
}

Engine::Engine(void* engine) {
	internal = engine;
}

Engine::Engine(Engine& engine) {
	nt_engine_decref((ntEngine*) internal);
	internal = engine.internal;
	nt_engine_incref((ntEngine*) internal);
}

Engine::~Engine() {
	internal = nt_engine_decref((ntEngine*) internal);
}

bool Engine::initialize(const char *name_or_path) {
	nt_engine_decref((ntEngine*) internal);
	return (internal = nt_engine_init(name_or_path));
}

const char *Engine::getName() {
	return nt_engine_get_name((ntEngine*) internal);
}

Value Engine::newGlobal() {
	return nt_engine_new_global((ntEngine*) internal, NULL);
}

Value Engine::newGlobal(Value global) {
	return nt_engine_new_global((ntEngine*) internal, global.borrowCValue());
}
