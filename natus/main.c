#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "natus.h"

bool func(ntValue **ret,
		  ntValue  *glb,
          ntValue  *ths,
          ntValue  *fnc,
          ntValue **arg,
          void     *msc) {
	const ntValue *str = nt_property_get(ths, "foo");
	if (str) printf("%s\n", nt_as_string(str));

	nt_function_return(ret, ARG_INTEGER(12345));
	return true;
}

int main(int argc, char** argv) {
	char *dbstring = NULL;
//	void *dbhandle = NULL;
	int   c = 0;

	// Parse our arguments
	while ((c = getopt (argc, argv, "d:")) != -1) {
		switch (c) {
			case 'd':
				while (*optarg == ' ') optarg++;
				dbstring = optarg;
				break;
			default:
				abort();
		}
	}

/*	if (dbstring) {
		dbhandle = natus_db_open(dbstring);
		if (!dbhandle) {
			fprintf(stderr, "Unable to open DB ('%s')!\n", dbstring);
			return 1;
		}
	} */

	ntValue *glbl = nt_global_init(NULL);
	if (!glbl) return 1;

	if (!nt_property_set(glbl, "import", ARG_FUNCTION(nt_import, NULL)))
		goto error;

	nt_evaluate(glbl, "import('python');", NULL, 0);

	nt_global_free(glbl);

	//natus_db_close(dbhandle);
	return 0;

	error:
		nt_global_free(glbl);
		return 1;
}
