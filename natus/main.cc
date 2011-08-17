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
#include <set>
#include <vector>
#include <string>
using namespace std;

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <sys/stat.h>
#include <sys/types.h>
#include <regex.h>

#include <readline/readline.h>
#include <readline/history.h>

#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#include <natus/natus.hh>
using namespace natus;

#define _str(s) # s
#define __str(s) _str(s)

#define PWSIZE 1024
#define PROMPT ">>> "
#define NPRMPT "... "
#define HISTORYFILE ".natus_history"
#define error(status, num, ...) { \
        fprintf(stderr, __VA_ARGS__); \
        return status; \
}

Value* glbl;

static bool inblock (const char *str) {
	char strstart = '\0';
	int bracedepth = 0, parendepth = 0;
	for (char prv = '\0' ; *str ; prv = *str++) {
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
				bracedepth++;
				break;
			case '}':
				// We're ending an existing block
				bracedepth--;
				break;
			case '(':
				// We're starting a new block
				parendepth++;
				break;
			case ')':
				// We're ending an existing block
				parendepth--;
				break;
		}
	}
	return bracedepth > 0 || parendepth > 0;
}

static vector<string> pathparser (string path) {
	if (path.find (':') == string::npos) {
		vector<string> tmp;
		if (path != "")
			tmp.push_back (path);
		return tmp;
	}

	string segment = path.substr (path.find_last_of (':') + 1);
	vector<string> paths = pathparser (path.substr (0, path.find_last_of (':')));
	if (segment != "")
		paths.push_back (segment);
	return paths;
}

static Value recursive_enumerate (Value obj, size_t depth = 0) {
	if (depth > 100)
		return obj.newArray (); // Guard against infinite loop

	// Get the object's properties
	Value *argv[2] = { &obj, NULL };
	Value args = obj.newArray (argv);
	Value names = glbl->get ("Object").call ("getOwnPropertyNames", args);

	// Recurse through the prototype
	Value proto = obj.get ("__proto__");
	if (!proto.isUndefined () && !proto.isNull()) {
		proto = recursive_enumerate (proto, depth + 1);
		long olen = names.get ("length").to<long> ();
		long plen = proto.get ("length").to<long> ();
		for (long i = 0 ; i < plen ; i++)
			names.set (olen++, proto[i]);
	}

	// Filter out undefined names
	Value validnames = obj.newArray ();
	long len = names.get ("length").to<long> ();
	for (long i = 0, cur = 0 ; i < len ; i++)
		if (!obj.get (names[i]).isUndefined ())
			validnames.set (cur++, names[i]);

	return validnames;
}

static char* completion_generator (const char* text, int state) {
	static set<string> names;
	static set<string>::iterator it;

	// Find the last character for our eval
	const char* last = NULL;
	for (ssize_t i = strlen (text) - 1 ; i >= 0 ; i--) {
		if (text[i] == ']')
			last = text + i + 1;
		if (text[i] == '.' || text[i] == '[')
			last = text + i;
		if (last)
			break;
	}

	// If this is the first call, build our names set
	if (state == 0) {
		names.clear (); // Clear the set

		Value obj = *glbl;
		if (last) {
			char* base = new char[last - text + 1];
			memset (base, 0, sizeof(char) * (last - text + 1));
			strncpy (base, text, last - text);
			obj = obj.evaluate (base, "", 0);
			delete[] base;
			if (obj.isUndefined () || obj.isException ())
				return NULL;
		}

		Value nm = recursive_enumerate (obj);
		for (long i = 0 ; i < nm.get ("length").to<long> () ; i++) {
			string sname = nm[i].to<UTF8> ();
			const char *name = sname.c_str ();

			// .property and GlobalObject properties (no '.')
			regex_t preg;
			if (!regcomp (&preg, "^[a-zA-Z_$][0-9a-zA-Z_$]*$", REG_EXTENDED)) {
				if (!regexec (&preg, name, 0, NULL, 0))
					names.insert ((last ? string (".") : string ("")) + sname);
				regfree (&preg);
			}

			// [N] and ["property"]
			if (last && !regcomp (&preg, "^[0-9]+$", REG_EXTENDED)) {
				if (!regexec (&preg, name, 0, NULL, 0))
					names.insert (string ("[") + sname + "]");
				else
					names.insert (string ("['") + sname + "']");
				regfree (&preg);
			}
		}
		it = names.begin ();
	}

	for (; it != names.end () ; it++) {
		if ((last && it->find (last) == 0) || (!last && it->find (text) == 0)) {
			string tmp = last ? (string (text) + (it->c_str () + strlen (last))) : *it;
			char* ret = strdup (tmp.c_str ());
			it++;
			return ret;
		}
	}

	return NULL;
}

static char** completion (const char* text, int start, int end) {
	return rl_completion_matches (text, completion_generator);
}

static Value set_path (Value& ctx, Require::HookStep step, char* name, void* misc) {
	if (step == Require::HookStepProcess && !strcmp (name, "system")) {
		Value args = ctx.get ("args");
		if (!args.isArray ())
			return NULL;

		const char** argv = (const char **) misc;
		for (int i = 0 ; argv[i] ; i++)
			arrayBuilder (args, argv[0]);
	}

	return NULL;
}

int main (int argc, char** argv) {
	Value global;
	vector<string> configs;
	const char *eng = NULL;
	const char *eval = NULL;
	int c = 0, exitcode = 0;
	glbl = &global;

	// The engine argument can be embedded in a symlink
	if (strstr (argv[0], "natus-") == argv[0])
		eng = strchr (argv[0], '-') + 1;

	// Get the path
	string path;
	if (getenv ("NATUS_PATH"))
		path = string (getenv ("NATUS_PATH")) + ":";
	path += __str(MODULEDIR);

	opterr = 0;
	while ((c = getopt (argc, argv, "+C:c:e:n")) != -1) {
		switch (c) {
			case 'C':
				configs.push_back (optarg);
				break;
			case 'c':
				eval = optarg;
				break;
			case 'e':
				eng = optarg;
				break;
			case 'n':
				path.clear ();
				break;
			case '?':
				if (optopt == 'e')
					fprintf (stderr, "Option -%c requires an engine name.\n", optopt);
				else {
					fprintf (stderr, "Usage: %s [-C <config>=<jsonval>|-c <javascript>|-e <engine>|-n|<scriptfile>]\n\n", argv[0]);
					fprintf (stderr, "Unknown option `-%c'.\n", optopt);
				}
				return 1;
			default:
				abort ();
		}
	}

	// Setup our global
	global = Value::newGlobal (eng);
	if (global.isUndefined () || global.isException ())
		error(2, 0, "Unable to init global!\n");

	// Setup our config
	Value cfg = global.newObject ();
	if (!path.empty ()) {
		while (path.find (':') != string::npos)
			path = path.substr (0, path.find (':')) + "\", \"" + path.substr (path.find (':') + 1);
		assert(!cfg.setRecursive("natus.require.path", fromJSON(global, "[\"" + path + "\"]"), Value::PropAttrNone, true).isException());
	}
	for (vector<string>::iterator it = configs.begin () ; it != configs.end () ; it++) {
		if (access (it->c_str (), R_OK) == 0) {
			ifstream file (it->c_str ());
			for (string line ; getline (file, line) ;) {
				if (line.find ('=') == string::npos)
					continue;
				string key = line.substr (0, line.find ('='));
				Value val = fromJSON (global, line.substr (line.find ('=') + 1).c_str ());
				if (val.isException ()) {
					fprintf (stderr, "An error occurred in file '%s'\n", it->c_str ());
					fprintf (stderr, "\tline: %s\n", line.c_str ());
					fprintf (stderr, "\t%s\n", val.to<UTF8> ().c_str ());
					continue;
				}
				assert(!cfg.setRecursive(key, val, Value::PropAttrNone, true).isException());
			}
			file.close ();
		} else if (it->find ("=") != string::npos) {
			string key = it->substr (0, it->find ('='));
			Value val = fromJSON (global, it->substr (it->find ('=') + 1));
			if (val.isException ()) {
				fprintf (stderr, "An error occurred on '%s'\n", it->c_str ());
				fprintf (stderr, "\t%s\n", val.to<UTF8> ().c_str ());
				continue;
			}
			assert(!cfg.setRecursive(key, val, Value::PropAttrNone, true).isException());
		}
	}

	// Bring up the Module Loader
	Require req (global);
	if (!req.initialize (cfg))
		error(3, 0, "Unable to init module loader\n!");

	// Do the evaluation
	if (eval) {
		Value res = global.evaluate (eval);

		// Print uncaught exceptions
		if (res.isException ())
			error(8, 0, "Uncaught Exception: %s", res.to<UTF8>().c_str());

		// Let the script return exitcodes
		if (res.isNumber ())
			exitcode = (int) res.to<long> ();

		return exitcode;
	}

	// Start the shell
	if (optind >= argc) {
		fprintf (stderr, "Natus v" PACKAGE_VERSION " - Using: %s\n", global.getEngineName());
		if (getenv ("HOME"))
			read_history ((string (getenv ("HOME")) + "/" + HISTORYFILE).c_str ());
		rl_readline_name = (char*) "natus";
		rl_attempted_completion_function = completion;
		rl_basic_word_break_characters = "`@><=;|&{(";
		read_history (HISTORYFILE);

		char* line;
		const char* prompt = PROMPT;
		string prev = "";

		long lcnt = 0;
		for (line = readline (prompt); line ; line = readline (prompt)) {
			// Strip the line
			string l = line;
			free (line);
			string::size_type start = l.find_first_not_of (" \t");
			l = start == string::npos ? string ("") : l.substr (start, l.find_last_not_of (" \t") - start + 1);
			if (l == "")
				continue;

			// Append the line to the buffer
			prev.append (l);

			// Continue to the next line
			if (inblock (prev.c_str ())) {
				prompt = NPRMPT;
				continue;
			}
			prompt = PROMPT;
			add_history (prev.c_str ());

			Value res = global.evaluate (prev.c_str (), "", lcnt++);
			string fmt;
			if (res.isException ()) {
				fmt = "Uncaught Exception: %s\n";
				if (prev == "exit" || prev == "quit") {
					res = res.newUndefined ();
					fprintf (stderr, "Use Ctrl-D (i.e. EOF) to exit\n");
				}
			} else
				fmt = "%s\n";

			if (!res.isUndefined () && !res.isNull ())
				fprintf (stderr, fmt.c_str (), res.to<UTF8> ().c_str ());
			prev = "";
		}
		printf ("\n");
		if (getenv ("HOME"))
			write_history ((string (getenv ("HOME")) + "/" + HISTORYFILE).c_str ());
		return 0;
	}

	// Get the script filename
	char *filename = argv[optind];

	// Stat our file
	struct stat st;
	if (stat (filename, &st))
		error(3, 0, "Script file does not exist: %s\n", filename);

	// Read in our script file
	FILE *file = fopen (filename, "r");
	if (!file)
		error(4, 0, "Unable to open file: %s\n", filename);
	char *jscript = new char[st.st_size + 1];
	if (!jscript) {
		fclose (file);
		error(5, 0, "Out of memory!\n");
	}
	if ((size_t) fread (jscript, 1, st.st_size, file) != (size_t) st.st_size) {
		delete[] jscript;
		fclose (file);
		error(6, 0, "Error reading file: %s\n", filename);
	}
	fclose (file);
	jscript[st.st_size] = '\0';

	// Evaluate it
	req.addHook ("system_args", set_path, argv + optind);
	Value res = global.evaluate (jscript, filename ? filename : "");
	delete[] jscript;

	// Print uncaught exceptions
	if (res.isException ())
		error(8, 0, "Uncaught Exception: %s\n", res.to<UTF8>().c_str());

	// Let the script return exitcodes
	if (res.isNumber ())
		exitcode = (int) res.to<long> ();

	return exitcode;
}
