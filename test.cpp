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
#include "llvm_svcs.h"

int main(int ac, char **av)
{
	int rc = -1;

	vector<string> taggers;

	if(ac > 1 && !strcmp(av[1], "tagging")) {
		IntervalMgr mgr;

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

		printf("invoking tagger %s on %s\n", 
			taggers[0].c_str(), pathTarget);
		if(0 != tagging_tag(pathTarget, taggers[1], mgr)) {
			printf("ERROR: tagging_tag()\n");
			goto cleanup;
		}
		
		mgr.print();
	}

	//if(!strcmp(av[1], "asmmem")) {
	if(1) {
		string bytes, err;

		/* run this and watch memory */
		const char *x86z =
			"main: # comment\n"
			"pushl	%ebp\n"
			"movl	%esp, %ebp\n"
			"subl	$12, %esp\n"
			"movl	12(%ebp), %eax\n"
			"movl	8(%ebp), %ecx\n"
			"movl	$0, -4(%ebp)\n"
			"movl	%ecx, -8(%ebp)\n"
			"movl	%eax, -12(%ebp)\n"
			"movl	-8(%ebp), %eax\n"
			"addl	$42, %eax\n"
			"addl	$12, %esp\n"
			"popl	%ebp\n"
			"retl\n"
			"jmp main\n";
		
		const char *x86 = "retl\n";

		llvm_svcs_init();

		for(int i=0; i<100; ++i) {
			if(	llvm_svcs_assemble(
				x86,
				LLVM_SVCS_DIALECT_ATT,
				"i386-none-none",
				LLVM_SVCS_CM_DEFAULT,
				LLVM_SVCS_RM_STATIC,
				NULL,
				bytes, err
			))
			{
				printf("ERROR! %s\n", err.c_str());
				break;
			}

			printf("it assembled! on test %d\n", i);
		}
	}

	rc = 0;
	cleanup:
	return 0;
}
