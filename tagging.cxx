/* this deals with tagging modules

	hlab gets its results from this functionality in the form of an interval manager */

/* c stdlib includes */
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

/* OS */
#include <sys/mman.h>
#include <dirent.h>
#include <unistd.h>
#include <signal.h>

/* c++ includes */
#include <map>
#include <string>
#include <vector>
using namespace std;

/* autils */
extern "C" {
    #include <autils/bytes.h>
    #include <autils/subprocess.h>
}
#include <autils/filesys.hpp>

/* local stuff */
#include "IntervalMgr.h"

int
tagging_tag(string target, string tagger, IntervalMgr &mgr)
{
	//TODO: check for stderr chars

    int rc = -1;
	
	/* subprocess variables */
	pid_t pid = -1;
	int child_stdout = -1;
	char arg0[PATH_MAX];
	char arg1[PATH_MAX];
	char *argv[3] = {arg0, arg1, NULL};

	/* file/line stuff */
	FILE *fp = NULL;
	char *line = NULL;

	/* execute */
	strncpy(arg0, tagger.c_str(), PATH_MAX-1);
	strncpy(arg1, target.c_str(), PATH_MAX-1);
	if(0 != launch_ex(arg0, argv, &pid, NULL, &child_stdout, NULL)) {
		printf("ERROR! launch_ex()\n");
		goto cleanup;
	}

	/* wrap the tagger's stdout descriptor into a convenient file pointer */
	fp = fdopen(child_stdout, "r");
	if(!fp) {
		printf("ERROR! fdopen()\n");
		goto cleanup;
	}

	if(mgr.readFromFilePointer(fp)) {
		printf("ERROR! readFromFilePointer()\n");
		goto cleanup;
	}

    rc = 0;
    cleanup:

	/* terminate tagger */
	if(pid != -1) {	
		int stat;

		printf("kill() on pid=%d...\n", pid);
		if(0 != kill(pid, SIGTERM)) 
			printf("ERROR: kill()\n");

		printf("waitpid() on pid=%d\n", pid);	
		if(waitpid(pid, &stat, 0) != pid)
			 printf("ERROR: waitpid()\n");

		pid = -1;
	}

    /* this free must occur even if getline() failed */
    if(line) {
        free(line);
        line = NULL;
    }

	/* close wrapping file pointer */
    if(fp) {
        fclose(fp);
        fp = NULL;
		/* The file descriptor is not dup'ed, and will be closed 
			when the stream created by fdopen() is closed. */
		child_stdout = -1;
    }

	/* close stdout file descriptor (if necessary) */
	if(child_stdout != -1) {
		close(child_stdout);
		child_stdout = -1;
	}

    return rc;
}

/* "what taggers exist?" */
int tagging_findall(vector<string> &result)
{
	string cwd;
	filesys_cwd(cwd);

	/* collect all files named hltag_* from hardcoded paths */
	filesys_ls(AUTILS_FILESYS_LS_STARTSWITH, "hltag_", cwd, result, true);
	filesys_ls(AUTILS_FILESYS_LS_STARTSWITH, "hltag_", cwd+"/taggers", 
		result, true);
	filesys_ls(AUTILS_FILESYS_LS_STARTSWITH, "hltag_", "/usr/local/bin", 
		result, true);

	return 0;
}

/* "what taggers exist that will agree to service <target>?" */
int tagging_pollall(string target, vector<string> &results)
{
	int rc = -1;

	/* collect all taggers */
	vector<string> candidates;
	if(0 != tagging_findall(candidates))
		goto cleanup;

	/* execute each one with target as an argument, see who responds */
	for(auto i=candidates.begin(); i!=candidates.end(); ++i) {
		#define CHILD_STDOUT_SZ 100
		int child_ret;
		char child_stdout[CHILD_STDOUT_SZ] = {'\0'};
		char arg0[PATH_MAX];
		char arg1[PATH_MAX];
		strncpy(arg0, i->c_str(), PATH_MAX-1);
		strncpy(arg1, target.c_str(), PATH_MAX-1);
		char *argv[3] = {arg0, arg1, NULL};

		string basename;
		filesys_basename(*i, basename);

		memset(child_stdout, 0, CHILD_STDOUT_SZ);
		if(0 != launch(arg0, argv, &child_ret, child_stdout, CHILD_STDOUT_SZ, NULL, 0)) {
			printf("ERROR: launch()\n");
			continue;
		}

		if(child_ret != 0) {
			//printf("tagger(%s) returned %d\n", basename.c_str(), 
			//	child_ret);
			continue;
		}

		if(strncmp(child_stdout, "[0x", 3)) {
			child_stdout[CHILD_STDOUT_SZ-1] = '\0';
			printf("ERROR: tagger(%s) output didn't look right (\"%s...\")\n",
				i->c_str(), child_stdout);
			continue;
		}
			
		results.push_back(*i);
	}

	rc = 0;
	cleanup:
	return rc;
}


