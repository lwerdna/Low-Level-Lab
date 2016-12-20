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

int get_py_ver(string &ver)
{
	int i, rc, rc_sub, len;
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

int get_taggers_from_dir(string path, vector<string> &results)
{
	DIR *dp;
	struct dirent *ep;

	dp = opendir(path.c_str());
	if(dp == NULL) return -1;
	
	while((ep = readdir(dp))) {
		int len = strlen(ep->d_name);
		if(len < 9) continue; /* needs "hlab_" and "X" and ".py" */
		if(0 != strncmp(ep->d_name, "hlab_", 5)) continue; /* start with "hlab_" */
		if(0 != strcmp((ep->d_name)+len-3, ".py")) continue; /* end with ".py" */
		
		string tmp = path + "/" + ep->d_name;
		results.push_back(tmp);
	}

	closedir(dp);

	return 0;
}

int get_cwd(string &cwd)
{
	char buf[PATH_MAX];
	if(buf != getcwd(buf, PATH_MAX)) return -1;
	cwd = buf;
	return 0;
}

int get_tagger_files(vector<string> &taggers)
{
	vector<string> tmp;

	/* collect from current dir (aid experimentation)
		and ./taggers (aid development) */
	string cwd;
	if(0==get_cwd(cwd)) {
		get_taggers_from_dir(cwd, tmp);
		get_taggers_from_dir(cwd+"/taggers", tmp);
	}

	/* collect from installed directory */
	string py_ver;
	if(0==get_py_ver(py_ver)) {
		get_taggers_from_dir("/usr/local/lib/python" + py_ver + "/site-packages", tmp);
	}

	/* append to result */
	taggers.insert(taggers.end(), tmp.begin(), tmp.end());

	return 0;
}

int main(int ac, char **av)
{
	int rc = -1, i;

	vector<string> taggers;

	string ver = "ERROR";
	if(0==get_py_ver(ver)) {
		printf("apython version: %s\n", ver.c_str());
	}
	if(0==get_py_ver(ver)) {
		printf("bpython version: %s\n", ver.c_str());
	}
	if(0==get_py_ver(ver)) {
		printf("cpython version: %s\n", ver.c_str());
	}

	get_tagger_files(taggers);
	for(auto i=taggers.begin(); i!=taggers.end(); ++i)
		printf("tagger: %s\n", (*i).c_str());

	rc = 0;
	cleanup:
	return 0;
}
