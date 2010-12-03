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

static Value alert(Value& ths, Value& fnc, vector<Value>& args, void *msc) {
	if (args.size() < 1)
		return ths.newString("Invalid number of arguments!").toException();
	fprintf(stderr, "%s\n", args[0].toString().c_str());
	return ths.newUndefined();
}

static Value dir(Value& ths, Value& fnc, vector<Value>& args, void *msc) {
	Value obj = ths.getGlobal();
	if (args.size() > 0)
		obj = args[0];
	set<string> names = obj.enumerate();
	for (set<string>::iterator i=names.begin() ; i != names.end() ; i++)
		fprintf(stderr, "\t%s\n", i->c_str());
	return ths.newUndefined();
}

bool inblock(const char *str) {
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

vector<string> pathparser(string path) {
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

int main(int argc, char** argv) {
	const char *eng = NULL;
	const char *eval = NULL;
	int c=0, exitcode=0;

	vector<string> path = pathparser(string(__str(MODULEDIR)) + ":" + (getenv("NATUS_PATH") ? getenv("NATUS_PATH") : ""));
	vector<string> whitelist = pathparser(getenv("NATUS_WHITELIST") ? getenv("NATUS_WHITELIST") : "");

	opterr = 0;
	while ((c = getopt (argc, argv, "c:e:n")) != -1) {
		switch (c) {
		case 'c':
			eval = optarg;
			break;
		case 'e':
			eng = optarg;
			break;
		case 'n':
			path.erase(path.begin());
			break;
		case '?':
			if (optopt == 'e')
				fprintf(stderr, "Option -%c requires an engine name.\n", optopt);
			else {
				fprintf(stderr, "Usage: %s [-c <javascript>|-e <engine>|-n|<scriptfile>]\n\n", argv[0]);
				fprintf(stderr, "Unknown option `-%c'.\n", optopt);
			}
			return 1;
		default:
			abort();
	   }
	}

	Engine engine;
	if (!engine.initialize(eng))
		error(1, 0, "Unable to init engine!");

	Value global = engine.newGlobal(path, whitelist);
	if (global.isUndefined())
		error(2, 0, "Unable to init global!");
	global.set("alert", global.newFunction(alert, NULL, NULL));
	global.set("dir",   global.newFunction(dir,   NULL, NULL));

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
		fprintf(stderr, "Natus " PACKAGE_VERSION " - Using: %s\n", engine.getName().c_str());

		char* line;
		const char* prompt = PROMPT;
		string prev = "";

		long lcnt = 0;
		for (line=readline(prompt) ; line ; line=readline(prompt)) {
			// Strip the line
			string l = line;
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
