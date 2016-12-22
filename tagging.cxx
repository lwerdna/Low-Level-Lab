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

/*****************************************************************************/
/* TAGGING RESULTS: LOAD FILE ON DISK -> Interval Manager */
/*****************************************************************************/

int
tagging_load_results(const char *fpath, IntervalMgr &mgr)
{
    int rc = -1;
    char *line = NULL;

    FILE *fp = fopen(fpath, "r");
    if(!fp) {
        printf("ERROR: fopen()\n");
        goto cleanup;
    }

    for(int line_num=1; 1; ++line_num) {
        size_t line_len;

        if(line) {
            free(line);
            line = NULL;
        }
        if(getline(&line, &line_len, fp) <= 0) {
            // error or EOF? don't squawk
            break;
        }

        if(0 == strcmp(line, "\x0d\x0a") || 0 == strcmp(line, "\x0a")) {
            continue;
        }

        /* remove trailing newline */
        while(line[line_len-1] == '\x0d' || line[line_len-1] == '\x0a') {
            line[line_len-1] = '\x00';
            line_len -= 1;
        }

        //printf("got line %d (len:%ld): -%s-", line_num, line_len, line);

        uint64_t start, end;
        uint32_t color;
        int oStart=-1, oEnd=-1, oColor=-1, oComment=-1;
        if(line[0] != '[') {
            printf("ERROR: expected '[' on line %d: %s\n", line_num, line);
            continue;
        }

        oStart = 1;
        /* seek comma (ending the start address) */
        for(int i=oStart; i<line_len; ++i) {
            if(line[i]==',') {
                line[i]='\x00';
                oEnd = i+1;
                break;
            }
        }

        if(oEnd == -1 || oEnd >= (line_len-1)) {
            printf("ERROR: missing or bad comma on line %d: %s\n", line_num, line);
            continue;
        }

        if(0 != parse_uint64_hex(line + oStart, &start)) {
            printf("ERROR: couldn't parse start address on line %d: %s\n", line_num, line);
            continue;
        }

        /* seek ') ' (ending the end address) */
        for(int i=oEnd; i<line_len; ++i) {
            if(0 == strncmp(line + i, ") ", 2)) {
                line[i]='\x00';
                oColor = i+2;
                break;
            }
        }

        if(oColor == -1 || oColor >= (line_len-1)) {
            printf("ERROR: missing or bad \") \" on line %d: %s\n", line_num, line);
            continue;
        }

        if(0 != parse_uint64_hex(line + oEnd, &end)) {
            printf("ERROR: couldn't parse end address on line %d: %s\n", line_num, line);
            continue;
        }
       
        /* seek next space or null (ending the color) */
        for(int i=oColor; 1; ++i) {
            if(0 == strncmp(line + i, " ", 1) || line[i]=='\x00') {
                line[i]='\x00';
                oComment = i+1;
                break;
            }
        }

        if(oComment >= line_len || strlen(line+oComment) == 0) {
            // missing comment, run it into null byte
            oComment = oColor-1;
        }

        if(0 != parse_uint32_hex(line + oColor, &color)) {
            printf("ERROR: couldn't parse color address on line %d: %s\n", line_num, line);
            continue;
        }

        /* done, add interval */
        mgr.add(Interval(start, end, string(line + oComment)));
    }

    rc = 0;
    cleanup:

    /* this free must occur even if getline() failed */
    if(line) {
        free(line);
        line = NULL;
    }

    if(fp) {
        fclose(fp);
        fp = NULL;
    }

    return rc;
}

/*****************************************************************************/
/* TAG MODULE ENUMERATING FROM DISK */
/*****************************************************************************/

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

/* "what taggers will service target?"
*/
int tagging_pollall(string target, vector<string> &results)
{
	int rc = -1;

	/* collect all taggers */
	vector<string> candidates;
	if(0 != tagging_findall(candidates))
		goto cleanup;

	/* execute each one with target as an argument, see who responds */
	for(auto i=candidates.begin(); i!=candidates.end(); ++i) {
		int child_ret;
		char child_stdout[4] = {0};
		char arg0[PATH_MAX];
		char arg1[PATH_MAX];
		strncpy(arg0, i->c_str(), PATH_MAX-1);
		strncpy(arg1, target.c_str(), PATH_MAX-1);
		char *argv[3] = {arg0, arg1, NULL};

		if(0 != launch(arg0, argv, &child_ret, child_stdout, 3, NULL, 0)) {
			printf("ERROR: launch()\n");
			continue;
		}

		if(child_ret != 0) {
			printf("ERROR: tagger returned %d\n", child_ret);
			continue;
		}

		if(strncmp(child_stdout, "[0x", 3)) {
			string basename;
			filesys_basename(*i, basename);
			child_stdout[3] = '\0';
			printf("ERROR: tagger output (%s) didn't look right (\"%s...\")\n",
				basename.c_str(), child_stdout);
			continue;
		}
			
		results.push_back(*i);
	}

	rc = 0;
	cleanup:
	return rc;
}

/*****************************************************************************/
/* TAG MODULE TagReally() */
/*****************************************************************************/

/* call <module>.tagReally() on pathTarget, writing results to pathTag */
int
tagging_module_exec(string pathModule, string pathTarget, string pathTags)
{
	int rc = -1;
	rc = 0;
	//cleanup:
	return rc;
}

// TODO: figure out the Py_DECREF stuff


