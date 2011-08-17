#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>

int onEngine (const char *engine, int argc, const char **argv);

int main (int argc, const char **argv) {
	int retval = 0;

	DIR* dir = opendir(ENGINEDIR);
	if (!dir)
		return 1;

	struct dirent *ent = NULL;
	while ((ent = readdir (dir)) && retval == 0) {
		if (retval != 0)
			break;

		size_t flen = strlen (ent->d_name);
		size_t slen = strlen (MODSUFFIX);

		if (!strcmp (".", ent->d_name) || !strcmp ("..", ent->d_name))
			continue;
		if (flen < slen || strcmp (ent->d_name + flen - slen, MODSUFFIX))
			continue;

		char *tmp = (char *) malloc(sizeof(char) * (flen + strlen(ENGINEDIR) + 2));
		if (!tmp) {
			retval = 1;
			break;
		}
		strcpy(tmp, ENGINEDIR);
		strcat (tmp, "/");
		strcat (tmp, ent->d_name);

		retval = onEngine (tmp, argc, argv);
		free(tmp);
	}

	closedir( dir);
	return retval;
}
