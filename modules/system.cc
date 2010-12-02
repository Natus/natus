#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>

#define I_ACKNOWLEDGE_THAT_NATUS_IS_NOT_STABLE
#include <natus/natus.h>
using namespace natus;

static bool setup_argv(Value& base) {
    int size;
    char buffer[4*1024]; // This is the maximum size of a procfile
	char procfile[36];

    // Open the procfile
	snprintf(procfile, 36, "/proc/%d/cmdline", getpid());
	FILE *cmdline = fopen(procfile, "r");
	if (!cmdline) return false;

	// Read the file
	size = fread(buffer, 1, 4*1024, cmdline);
	fclose(cmdline);

	// Count the items
	char *start = buffer;
	vector<Value> params;
	for (int i=0 ; i < size ; i++)
		if (buffer[i] == '\0') {
			params.push_back(base.newString(start));
			start = buffer+i+1;
		}

	return base.set("exports.args", base.newArray(params));
}

class EnvClass : public Class {
public:
	EnvClass() : Class(Class::Object) {}

	virtual bool del(Value& obj, string name) {
		unsetenv(name.c_str());
		return true;
	}

	virtual Value get(Value& obj, string name) {
		char *value = getenv(name.c_str());
		return value ? obj.newString(value) : obj.newUndefined();
	}

	virtual bool set(Value& obj, string name, Value& value) {
		setenv(name.c_str(), value.toString().c_str(), true);
		return true;
	}

	virtual Value enumerate(Value& obj) {
		extern char **environ;

		vector<Value> keys;
		for (int i=0 ; environ[i] ; i++) {
			string key = environ[i];
			if (key.find_first_of('=') == string::npos) continue;
			key = key.substr(0, key.find_first_of('='));
			keys.push_back(obj.newString(key));
		}
		return obj.newArray(keys);
	}
};

extern "C" bool natus_require(Value& base) {
	bool ok = setup_argv(base);
	     ok = base.set("exports.env", base.newObject(new EnvClass())) || ok;
	return ok;
}
