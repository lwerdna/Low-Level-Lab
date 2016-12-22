/* os stuff */
#include <unistd.h> // pid_t
#include <dirent.h>

/* c stdlib */
#include <stdio.h>
#include <ctype.h> // isdigit()
#include <string.h>

/* c++ */
#include <string>
#include <vector>
using namespace std;

/* from autils */
extern "C" {
#include <autils/subprocess.h>
}

/* local stuff */
#include "IntervalMgr.h"
#include "tagging.h"

int main(int ac, char **av)
{
	int rc = -1;

	vector<string> taggers;

	if(ac < 2) goto cleanup;

	if(!strcmp(av[1], "tagging")) {

		printf("all taggers:\n");
		vector<string> taggers;
		if(0 != tagging_findall(taggers)) {
			printf("ERROR! tagging_findall()\n");
			goto cleanup;
		}
		for(auto i=taggers.begin(); i!=taggers.end(); ++i)
			printf("%s\n", i->c_str());
		

		/* third argument? have the taggers chew on something */
		if(ac < 3) {
			rc = 0;
			goto cleanup;
		}

		char *pathTarget = av[2];
		printf("these modules accept %s:\n", pathTarget);
		taggers.clear();
		if(0 != tagging_pollall(pathTarget, taggers)) {
			printf("ERROR! tagging_modules_test()\n");
			goto cleanup;
		}
		for(auto i=taggers.begin(); i!=taggers.end(); ++i) {
			printf("%s\n", i->c_str());
		}

		/* test tagging_module_exec() */
	}

	rc = 0;
	cleanup:
	return 0;
}
