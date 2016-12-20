/* os stuff */
#include <unistd.h> // pid_t

/* c stdlib */
#include <stdio.h>
#include <ctype.h> // isdigit()

/* c++ */
#include <string>
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
	char s_null[] = "\x00";
	char *argv[3] = {s_python, s_V, s_null};
	char *buf_ver = NULL;
	
	rc = launch(s_python, argv, &rc_sub, buf_stdout, 64, buf_stderr, 64);

	printf("buf_stdout: %s\n", buf_stdout);
	printf("buf_stderr: %s\n", buf_stderr);

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

int main(int ac, char **av)
{
	int rc = -1, i;

	string ver;

	printf("hello, world!\n");

	i = get_py_ver(ver);
	if(i) {
		printf("ERROR: get_py_ver() returned %d\n", i);
		goto cleanup;
	}

	printf("python version: %s\n", ver.c_str());

	rc = 0;
	cleanup:
	return 0;
}
