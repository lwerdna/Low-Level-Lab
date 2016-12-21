/* this deals with tagging modules

	it's separated out mainly to isolate hlab from python details

	hlab gets its results from this functionality in the form of an interval manager */

/* c stdlib includes */
#include <time.h>
#include <stdio.h>

/* OS */
#include <sys/mman.h>
#include <dirent.h>

/* c++ includes */
#include <map>
#include <string>
#include <vector>
using namespace std;

/* python */
#include <Python.h>

/* autils */
extern "C" {
    #include <autils/bytes.h>
    #include <autils/subprocess.h>
}

/* local stuff */
#include "IntervalMgr.h"

/* globals */
PyObject *pModImp = NULL;
PyObject *pFuncLoadSource = NULL;
/* convert "/usr/local/lib/python2.7/site-packages/hlab_pe64.py" to "hlab_pe64"
*/
int
tagging_path_to_module_name(string path, string &result)
{
	char buf[PATH_MAX];
	int len = path.size();
	if(len >= PATH_MAX) return -1;
	strcpy(buf, path.c_str());
	if(strlen(buf) != len) return -1;
	if(0 != strncmp(buf+len-3, ".py", 3)) return -1;
			
	/* cut off the extension */
	buf[len-3] = '\0';

	/* scan backwards from before ".py" */
	for(int i=len-4; i>0; --i) {
		/* find "hlab_" ? */
		if(0 == strncmp(buf+i, "hlab_", 5)) {
			result = buf+i;
			return 0;
		}
	}
	return -1;
}

/* initialize python to a nice state */
int
tagging_python_init(char *progName)
{
	int rc = -1;

	Py_SetProgramName(progName);
	Py_Initialize();

	pModImp = PyImport_ImportModule("imp");
	if(!pModImp) goto cleanup; 

	pFuncLoadSource = PyObject_GetAttrString(pModImp, "load_source");
	if(!pFuncLoadSource) goto cleanup;

	printf("pFuncLoadSource is: %p\n", pFuncLoadSource);

	rc = 0;
	cleanup:
	return rc;
}

/* load a module from a path */
int
tagging_python_load_tagger(string modulePath, PyObject **pModule,
  PyObject **pTagTest, PyObject **pTagReally)
{
	int rc = -1;
	PyObject *pArgs, *pStr0, *pStr1, *pRetVal;
	string moduleName;
	
	if(0 != tagging_path_to_module_name(modulePath, moduleName)) {
		goto cleanup;
	}

	/* - call imp.load_source("hlab_XXX", "/path/to/hlab_XXX.py") 
       - import hlab_XXX
	   NOTE: in Python3, reload() is in imp module, and in 3.4, imp is
	         deprecated in favor of importlib */
	pArgs = PyTuple_New(2);
	if(!pArgs) goto cleanup;
	pStr0 = PyString_FromString(moduleName.c_str());
	if(!pStr0) goto cleanup;
	pStr1 = PyString_FromString(modulePath.c_str());
	if(!pStr1) goto cleanup;
	PyTuple_SetItem(pArgs, 0, pStr0); /* SetItem *steals* reference, no decref from us */
	PyTuple_SetItem(pArgs, 1, pStr1);
	pRetVal = PyObject_CallObject(pFuncLoadSource, pArgs);
	if(!pRetVal) {
		printf("WTF! ");
		PyErr_Print();
		goto cleanup;
	}
	*pModule = PyImport_ImportModule(moduleName.c_str());
	if(!(*pModule)) goto cleanup; 

	/* resolve tagTest() and tagReally() */
	if(pTagTest) {
		*pTagTest = PyObject_GetAttrString(*pModule, "tagTest");
		if(!(*pTagTest)) goto cleanup;
	}
	if(pTagReally) {
		*pTagReally = PyObject_GetAttrString(*pModule, "tagReally");
		if(!(*pTagReally)) goto cleanup;
	}

	rc = 0;
	cleanup:
	return rc;
}



int
tagging_release()
{
	// TODO: free or decref imp?
	return 0;
}

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
int 
tagging_get_py_ver(string &ver)
{
	int rc, rc_sub;
	char buf_stdout[64] = {0};
	char buf_stderr[64] = {0};
	char s_python[] = "python";
	char s_V[] = "-V";
	char *argv[3] = {s_python, s_V, NULL};
	char *buf_ver = NULL;
	
	rc = launch(s_python, argv, &rc_sub, buf_stdout, 64, buf_stderr, 64);

	if(rc) return -1;
	if(rc_sub) return -2;
	/* prior to Python 3.4, version information went to stderr */
	if(*buf_stdout == 'P')
		buf_ver = buf_stdout;
	else if(*buf_stderr == 'P')
		buf_ver = buf_stderr;
	else
		return -3;

	/* now buf_ver points to either stdout or stderr buf */
	if(strncmp(buf_ver, "Python ", 7)) return -4;
	if(!isdigit(buf_ver[7])) return -5;
	if(buf_ver[8] != '.') return -6;
	if(!isdigit(buf_ver[9])) return -7;
	if(buf_ver[10] != '.') return -8;

	buf_ver[10] = '\0';
	
	ver = buf_ver + 7;

	return 0;
}

/* "what tagging modules are available from a specific directory?"
*/
int 
tagging_modules_find_dir(string path, vector<string> &results)
{
	DIR *dp;
	struct dirent *ep;

	dp = opendir(path.c_str());
	if(dp == NULL) return -1;
	
	while((ep = readdir(dp))) {
		int len = strlen(ep->d_name);

		/* file name must match r'hlab_.*\.py' */
		if(len < 9) continue; /* needs "hlab_" and "X" and ".py" */
		if(0 != strncmp(ep->d_name, "hlab_", 5)) continue; /* start with "hlab_" */
		if(0 != strcmp((ep->d_name)+len-3, ".py")) continue; /* end with ".py" */
		
		/* it must load */
		string pathed = path + "/" + ep->d_name;
		PyObject *a, *b, *c;
		if(0 != tagging_python_load_tagger(pathed, &a, &b, &c)) {
			// TODO: decref
			continue;
		}

		results.push_back(pathed);
	}

	closedir(dp);

	return 0;
}

/* "what tagging modules are available everywhere?"
*/
int tagging_modules_find_all(vector<string> &taggers)
{
	vector<string> tmp;
	char cwd[PATH_MAX];

	/* collect from current dir (aid experimentation)
		and ./taggers (aid development) */
	if(cwd == getcwd(cwd, PATH_MAX)) {
		tagging_modules_find_dir(cwd, tmp);
		tagging_modules_find_dir(string(cwd) + "/taggers", tmp);
	}

	/* collect from installed directory */
	string py_ver;
	if(0 == tagging_get_py_ver(py_ver)) {
		tagging_modules_find_dir("/usr/local/lib/python" + py_ver + "/site-packages", tmp);
	}

	/* append to result */
	taggers.insert(taggers.end(), tmp.begin(), tmp.end());

	return 0;
}

/*****************************************************************************/
/* TAG MODULE TEST() */
/*****************************************************************************/

/* "can /Users/john/Downloads/test.py tag /tmp/foo.exe?" 
	
	result is (true/false) */
int 
tagging_module_test(string modulePath, string pathTarget, bool &result)
{
	int rc = -1;

	PyObject *pRetVal, *pStr, *pArgs;
	PyObject *pTagger, *pTest;

	string moduleName;
	if(0 != tagging_path_to_module_name(modulePath, moduleName)) {
		goto cleanup;
	}
	   
	/* import tag module, resolve tagTest() */
	if(0 != tagging_python_load_tagger(modulePath, &pTagger, 
	  &pTest, NULL)) {
		goto cleanup;
	}

	/* call tagmodule.tagTest("/path/to/target.bin") */	
	pStr = PyString_FromString(pathTarget.c_str());
	if(!pStr) goto cleanup;
	pArgs = PyTuple_New(1);
	if(!pArgs) goto cleanup;
	PyTuple_SetItem(pArgs, 0, pStr);
	pRetVal = PyObject_CallObject(pTest, pArgs);
	if(!pRetVal) goto cleanup;

	/* can tag the file when tagTest() returns nonzero */
	result = (0 != PyInt_AsLong(pRetVal));

	rc = 0;
	cleanup:
	if(rc != 0) {
		PyErr_Print();
	}
	return rc;
}

/* "can any of [modPath0, modPath1, ...] tag /tmp/foo.exe?" 

	result is subset of modules that answered yes */
int
tagging_modules_test(vector<string> modPaths, string pathTarget,
	vector<string> &result)
{
	int rc = -1;

	for(auto i=modPaths.begin(); i!=modPaths.end(); ++i) {
		bool bCanTag = false;
		if(0 == tagging_module_test(*i, pathTarget, bCanTag)) {
			if(bCanTag) {
				result.push_back(*i);
			}
		}
	}

	rc = 0;
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

	PyObject *pMod=NULL;
	PyObject *pFunc;
	PyObject *pPathSrc=NULL, *pPathDst=NULL;
	PyObject *pValue=NULL;
	PyObject *pArgs=NULL;

	/* ensure that this tagging modules really supports the target, and load
		that module as "tagmodule" */
	bool supported;
	if(0 != tagging_module_test(pathModule, pathTarget, supported))
		goto cleanup;
	if(!supported)
		goto cleanup;

	/* - import tagmodule
	   - resolve tagReally()
	   - call tagReally(<pathTarget>, <pathTags>)	
	*/	
	pMod = PyImport_ImportModule("tagmodule");
	if(!pMod) goto cleanup;
	pFunc = PyObject_GetAttrString(pMod, "tagReally");
	if(!pFunc) goto cleanup;
	pPathSrc = PyString_FromString(pathTarget.c_str());
	if(!pPathSrc) goto cleanup;
	pPathDst = PyString_FromString(pathTags.c_str());
	if(!pPathDst) goto cleanup;
	pArgs = PyTuple_New(2);
	if(!pArgs) goto cleanup;
	PyTuple_SetItem(pArgs, 0, pPathSrc);
	PyTuple_SetItem(pArgs, 1, pPathDst);
	pValue = PyObject_CallObject(pFunc, pArgs);
	if(!pValue) goto cleanup;

	rc = 0;

	cleanup:

	if(rc) {
		PyErr_Print();
	}

	return rc;
}

// TODO: figure out the Py_DECREF stuff


