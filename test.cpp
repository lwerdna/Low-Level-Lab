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

/* python */
#include <Python.h>

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
		printf("initializing python...\n");
		Py_SetProgramName(av[0]);
		Py_Initialize();

		vector<string> modules;
		if(0 != tagging_modules_find_all(modules)) {
			printf("ERROR! tagging_modules_find_all()\n");
			goto cleanup;
		}

		for(auto i=modules.begin(); i!=modules.end(); ++i) {
			printf("module: %s\n", i->c_str());
		}

		string ver;
		if(0 != tagging_get_py_ver(ver)) {
			printf("ERROR! tagging_get_py_ver()\n");
			//goto cleanup;
		}

		printf("python version: %s\n", ver.c_str());

		/* third argument? have the taggers chew on something */
		if(ac < 3) {
			rc = 0;
			goto cleanup;
		}

		char *pathTarget = av[2];
		printf("target file: %s\n", pathTarget);
		vector<string> modulesAccepting;
		if(0 != tagging_modules_test(modules, pathTarget, modulesAccepting)) {
			printf("ERROR! tagging_modules_test()\n");
			goto cleanup;
		}

		printf("these modules accept your file:\n");
		for(auto i=modulesAccepting.begin(); i!=modulesAccepting.end(); ++i) {
			printf("accepting module: %s\n", i->c_str());
		}

		/* test tagging_module_exec() */
	}

	rc = 0;
	cleanup:
	return 0;
}
