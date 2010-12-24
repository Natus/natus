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

#include <iostream>
#include <fstream>
#include <algorithm>

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <error.h>
#include <sys/stat.h>

#include <readline/readline.h>
#include <readline/history.h>

#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#include <natus.h>
using namespace natus;

#define _str(s) # s
#define __str(s) _str(s)

#define PWSIZE 1024
#define PROMPT ">>> "
#define NPRMPT "... "
#define HISTORYFILE ".natus_history"

Value* glbl;

static Value alert(Value& ths, Value& fnc, vector<Value>& args) {
	if (args.size() < 1)
		return ths.newString("Invalid number of arguments!").toException();
	fprintf(stderr, "%s\n", args[0].toString().c_str());
	return ths.newUndefined();
}

static Value dir(Value& ths, Value& fnc, vector<Value>& args) {
	Value obj = ths.getGlobal();
	if (args.size() > 0)
		obj = args[0];
	set<string> names = obj.enumerate();
	for (set<string>::iterator i=names.begin() ; i != names.end() ; i++)
		fprintf(stderr, "\t%s\n", i->c_str());
	return ths.newUndefined();
}

static bool inblock(const char *str) {
	char strstart = '\0';
	int cnt = 0;
	for (char prv='\0' ; *str ; prv=*str++) {
		if (strstart) {
			if (strstart == '"' && *str == '"' && prv != '\\')
				strstart = '\0';
			else if (strstart == '\'' && *str == '\'' && prv != '\\')
				strstart = '\0';
			continue;
		}

		// We are not inside a string literal
		switch (*str) {
			case '"':
				// We're starting a double-quoted string
				strstart = '"';
				break;
			case '\'':
				// We're starting a single-quoted string
				strstart = '\'';
				break;
			case '{':
				// We're starting a new block
				cnt++;
				break;
			case '}':
				// We're ending an existing block
				cnt--;
				break;
		}
	}
	return cnt > 0;
}

static vector<string> pathparser(string path) {
	if (path.find(':') == string::npos) {
		vector<string> tmp;
		if (path != "")
			tmp.push_back(path);
		return tmp;
	}

	string segment = path.substr(path.find_last_of(':')+1);
	vector<string> paths = pathparser(path.substr(0, path.find_last_of(':')));
	if (segment != "") paths.push_back(segment);
	return paths;
}

static char* completion_generator(const char* text, int state) {
	static set<string> names;
	static set<string>::iterator it;
	char* last = (char*) strrchr(text, '.');

	if (state == 0) {
		Value obj = *glbl;
		if (last) {
			char* base = new char[last-text+1];
			memset(base, 0, sizeof(char) * (last-text+1));
			strncpy(base, text, last-text);
			obj = obj.get(base);
			delete[] base;
			if (obj.isUndefined()) return NULL;
		}

		names = obj.enumerate();
		it = names.begin();
	}
	if (last) last++;

	for ( ; it != names.end() ; it++) {
		if ((last && it->find(last) == 0) || (!last && it->find(text) == 0)) {
			string tmp = last ? (string(text) + (it->c_str()+strlen(last))) : *it;
			char* ret = strdup(tmp.c_str());
			it++;
			return ret;
		}
	}

	return NULL;
}

static char** completion(const char* text, int start, int end) {
	return rl_completion_matches(text, completion_generator);
}

Value set_path(Value& module, string& name, string& reldir, vector<string>& path, void* misc) {
	const char** argv = (const char **) misc;
	Value ret = module.newUndefined();
	if (name != "system") return ret;
	Value args = module.get("args");
	if (!args.isArray()) return ret;

	for (int i=0 ; argv[i] ; i++)
		args.push(module.newString(argv[i]));
	return ret;
}

int main(int argc, char** argv) {
	Engine engine;
	Value  global;
	vector<string> configs;
	const char *eng = NULL;
	const char *eval = NULL;
	int c=0, exitcode=0;
	glbl = &global;

	// The engine argument can be embedded in a symlink
	if (strstr(argv[0], "natus-") == argv[0])
		eng = strchr(argv[0], '-')+1;

	// Get the path
	string path;
	if (getenv("NATUS_PATH"))
		path = string(getenv("NATUS_PATH")) + ":";
	path += __str(MODULEDIR);

	opterr = 0;
	while ((c = getopt (argc, argv, "+C:c:e:n")) != -1) {
		switch (c) {
		case 'C':
			configs.push_back(optarg);
			break;
		case 'c':
			eval = optarg;
			break;
		case 'e':
			eng = optarg;
			break;
		case 'n':
			path.clear();
			break;
		case '?':
			if (optopt == 'e')
				fprintf(stderr, "Option -%c requires an engine name.\n", optopt);
			else {
				fprintf(stderr, "Usage: %s [-C <config>=<jsonval>|-c <javascript>|-e <engine>|-n|<scriptfile>]\n\n", argv[0]);
				fprintf(stderr, "Unknown option `-%c'.\n", optopt);
			}
			return 1;
		default:
			abort();
	   }
	}

	// Initialize the engine
	if (!engine.initialize(eng))
		error(1, 0, "Unable to init engine!");

	// Setup our config (using a temporary global)
	global = engine.newGlobal();
	if (global.isUndefined() || global.isException())
			error(2, 0, "Unable to init global!");
	Value cfg = global.newObject();
	if (!path.empty()) {
		while (path.find(':') != string::npos)
			path = path.substr(0, path.find(':')) + "\", \"" + path.substr(path.find(':')+1);
		cfg.set("natus.path", global.fromJSON("[\"" + path + "\"]"), Value::None, true);
	}
	for (vector<string>::iterator it=configs.begin() ; it != configs.end() ; it++) {
		if (access(it->c_str(), R_OK) == 0) {
			ifstream file(it->c_str());
			for (string line ; getline(file, line) ; ) {
				if (line.find('=') == string::npos) continue;
				string key = line.substr(0, line.find('='));
				Value  val = global.fromJSON(line.substr(line.find('=')+1));
				if (val.isException()) continue;
				cfg.set(key, val, Value::None, true);
			}
			file.close();
		} else if (it->find("=") != string::npos) {
			string key = it->substr(0, it->find('='));
			Value  val = global.fromJSON(it->substr(it->find('=')+1));
			if (val.isException()) continue;
			cfg.set(key, val, Value::None, true);
		}
	}
	string config = cfg.toJSON();

	// Create our (new) execution global
	global = engine.newGlobal(config);
	if (global.isUndefined() || global.isException())
		error(2, 0, "Unable to init global!");

	// Export some basic functions
	global.set("alert", global.newFunction(alert));
	global.set("dir",   global.newFunction(dir));

	// Do the evaluation
	if (eval) {
		Value res = global.evaluate(eval);

		// Print uncaught exceptions
		if (res.isException()) {
			string msg = res.toString();
			error(8, 0, "Uncaught Exception: %s", msg.c_str());
		}

		// Let the script return exitcodes
		if (res.isNumber())
			exitcode = (int) res.toLong();

		return exitcode;
	}

	// Start the shell
	if (optind >= argc) {
		fprintf(stderr, "Natus v" PACKAGE_VERSION " - Using: %s\n", engine.getName().c_str());
		if (getenv("HOME"))
			read_history((string(getenv("HOME")) + "/" + HISTORYFILE).c_str());
		rl_readline_name = "natus";
		rl_attempted_completion_function = completion;
		read_history(HISTORYFILE);

		char* line;
		const char* prompt = PROMPT;
		string prev = "";

		long lcnt = 0;
		for (line=readline(prompt) ; line ; line=readline(prompt)) {
			// Strip the line
			string l = line;
			free(line);
			string::size_type start = l.find_first_not_of(" \t");
			l = start == string::npos ? string("") : l.substr(start, l.find_last_not_of(" \t")-start+1);
			if (l == "") continue;

			// Append the line to the buffer
			prev.append(l);

			// Continue to the next line
		    if (inblock(prev.c_str())) {
		    	prompt = NPRMPT;
		    	continue;
		    }
		    prompt = PROMPT;
			add_history(prev.c_str());

			Value res = global.evaluate(prev, string(), lcnt++, false);
			string msg;
			string fmt;
			msg = res.toString();
			if (res.isException()) {
				fmt = "Uncaught Exception: %s\n";
				if (prev == "exit" || prev == "quit") {
					res = res.newUndefined();
					fprintf(stderr, "Use Ctrl-D (i.e. EOF) to exit\n");
				}
			} else
				fmt = "%s\n";

			if (!res.isUndefined() && !res.isNull())
				fprintf(stderr, fmt.c_str(), msg.c_str());
			prev = "";
		}
		printf("\n");
		if (getenv("HOME"))
			write_history((string(getenv("HOME")) + "/" + HISTORYFILE).c_str());
		return 0;
	}

	// Get the script filename
	char *filename = argv[optind];

	// Stat our file
	struct stat st;
	if (stat(filename, &st))
		error(3, 0, "Script file does not exist: %s", filename);

	// Read in our script file
	FILE *file = fopen(filename, "r");
	if (!file)
		error(4, 0, "Unable to open file: %s", filename);
	char *jscript = new char[st.st_size + 1];
	if (!jscript) {
		fclose(file);
		error(5, 0, "Out of memory!");
	}
	if ((size_t) fread(jscript, 1, st.st_size, file) != (size_t) st.st_size) {
		delete[] jscript;
		fclose(file);
		error(6, 0, "Error reading file: %s", filename);
	}
	fclose(file);
	jscript[st.st_size] = '\0';

	// Evaluate it
	global.addRequireHook(true, set_path, argv+optind);
	Value res = global.evaluate(jscript, filename ? filename : "");
	delete[] jscript;

	// Print uncaught exceptions
	if (res.isException()) {
		string msg = res.toString();
		error(8, 0, "Uncaught Exception: %s", msg.c_str());
	}

	// Let the script return exitcodes
	if (res.isNumber())
		exitcode = (int) res.toLong();

	return exitcode;
}
