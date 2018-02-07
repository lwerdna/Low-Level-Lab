#include <time.h>
#include <stdio.h>
#include <stdint.h>

#include <FL/Fl.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Display.H>

#include "ClabGui.h"


extern "C" {
#include <autils/crc.h>
#include <autils/filesys.h>
#include <autils/subprocess.h>
}

ClabGui *gui = NULL;

/* recompile state vars */
bool compile_forced = false;
bool compile_requested = false;
int length_last_compile = 0;
time_t time_last_compile = 0;
uint32_t crc_last_compile = 0;

/* temp files used for source, exec */
char cPath[128], cppPath[128], ePath[128];

/* buffs to receive compiler output */
#define STREAM_BUF_SIZE (1024*1024)
char *stdout_buf = NULL;
char *stderr_buf = NULL;

/*****************************************************************************/
/* UTILITIES */
/*****************************************************************************/
void outLogScrollToEnd(void)
{
    int n_chars = gui->outBuf->length();
    int n_lines = gui->outLog->count_lines(0, n_chars-1, true);
    gui->outLog->scroll(n_lines-1, 0);
}

/*****************************************************************************/
/* MAIN ROUTINE */
/*****************************************************************************/

void
compile()
{
    //printf("%s()\n", __func__);

    int rc = -1, rc_child, i;
    int n_args;
    char *argv[6];
    char *srcText = NULL;
    FILE *fp;
    char ca_empty[] = "";
    char ca_clang[] = "clang";
    char ca_clangpp[] = "clang++";
    char ca_otool[] = "otool";
    char ca_dash_t[] = "-t";
    char ca_dash_V[] = "-V";
    char ca_dash_v[] = "-v";
    char ca_dash_X[] = "-X";
    char ca_dash_o[] = "-o";

    /* compiler */
    bool isC;
    const char *compilerChoice;
    char *compilerPath;

    /* compile timing vars */
    time_t time_now;
    int length_now;
    uint32_t crc_now;

    Fl_Text_Buffer *srcBuf = gui->srcBuf;
    Fl_Text_Buffer *asmBuf = gui->asmBuf;
    Fl_Text_Buffer *outBuf = gui->outBuf;

    srcText = srcBuf->text();

    if(!compile_forced && !compile_requested) {
        //printf("skipping compile, neither forced nor requested\n");
        goto cleanup;
    }
    
    if(!compile_forced && compile_requested) {
        /* skip if we compiled within the last second */
        time(&time_now);
        float delta = difftime(time_now, time_last_compile);
        if(delta >= 0 && delta < 1) {
            /* just too soon, remain requested */
            //printf("skipping compile, too soon (now,last,diff)=(%ld,%ld,%f)\n",
            //    time_now, time_last_compile, difftime(time_now, time_last_compile));
            goto cleanup;
        }
        else {
            /* skip if the text is unchanged */
            length_now = srcBuf->length();
            if(length_last_compile == length_now) {
                crc_now = crc32(0, srcText, srcBuf->length());
                if(crc_now == crc_last_compile) {
                    //printf("skipping compile, buffer unchanged\n");
                    compile_requested = false;
                    goto cleanup;
                }
            }
        }
    }

    compile_forced = false;
    compile_requested = false;
    crc_last_compile = crc_now;
    time_last_compile = time_now;
    length_last_compile = length_now;

    /* compile follows thru... */

    /* form compiler command line, invoke compiler */
    i=0;
    compilerChoice = gui->icCompiler->input()->value();
    if(0 == strcmp(compilerChoice, "clang (in path)")) {
        isC = true;
        argv[i++] = ca_clang;
        compilerPath = ca_clang;
    }
    else if(0 == strcmp(compilerChoice, "clang++ (in path)")) {
        isC = false;
        argv[i++] = ca_clangpp;
        compilerPath = ca_clangpp;
    }
    else {
        printf("ERROR: unknown compiler: %s\n", compilerChoice);
        goto cleanup;
    }
    //argv[i++] = "-v";
    argv[i++] = isC ? cPath : cppPath;
    argv[i++] = ca_dash_o;
    argv[i++] = ePath;
    if(gui->btnVerbose->value()) argv[i++] = ca_dash_v;
    argv[i++] = (char *)gui->icOptimization->input()->value();
    argv[i++] = (char *)gui->icDebug->input()->value();
    argv[i++] = NULL;
    n_args = i;

    gui->clBuf->text("");
    for(i=0; i<n_args; ++i) {
        gui->clBuf->append(argv[i]);
        gui->clBuf->append(" ");
    }

    /* write source, clear buffers */
    fp = fopen(isC ? cPath : cppPath, "w");
    fwrite(srcText, 1, srcBuf->length(), fp);
    fclose(fp);

    memset(stdout_buf, '\0', STREAM_BUF_SIZE);
    memset(stderr_buf, '\0', STREAM_BUF_SIZE);

    /* launch it */
    char *compilerExecName;
    if(0 != launch(compilerPath, argv, &rc_child, stdout_buf, STREAM_BUF_SIZE,
        stderr_buf, STREAM_BUF_SIZE, 1))
    {
        printf("ERROR: launch()");
        goto cleanup;
    }

    /* stdout, stdout to the outLog */
    outBuf->text("");
    if(gui->btnStdout->value()) outBuf->append(stdout_buf);
    if(gui->btnStderr->value()) outBuf->append(stderr_buf);
    if(outBuf->length() == 0) outBuf->text("(no output)");

    /* default is NOT to auto scroll */
    if(gui->btnScroll->value()) outLogScrollToEnd();
    if(gui->btnWrap->value()) 
        gui->outLog->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
    else 
        gui->outLog->wrap_mode(Fl_Text_Display::WRAP_NONE, 0);
    
    /* disassemble? */
    asmBuf->text("");

    if(strstr(stdout_buf, "error") || strstr(stderr_buf, "error")) {
        printf("compile error detected, skipping disassembler\n");
        goto cleanup;
    }

    i=0;
    argv[i++] = ca_otool;
    argv[i++] = ca_dash_t;
    argv[i++] = ePath;
    argv[i++] = ca_dash_V;
    argv[i++] = ca_dash_X;
    argv[i++] = NULL;

    memset(stdout_buf, '\0', STREAM_BUF_SIZE);
    memset(stderr_buf, '\0', STREAM_BUF_SIZE);

    {
        printf("launching ");
        for(i=0; argv[i]!=NULL; ++i)
            printf("%s ", argv[i]);
        printf("\n");
    }
    if(0 != launch(ca_otool, argv, &rc_child, stdout_buf, STREAM_BUF_SIZE,
        stderr_buf, STREAM_BUF_SIZE, 1))
    {
        printf("ERROR: launch()");
        goto cleanup;
    }

	printf("raw stdout_buf:\n%s\n", stdout_buf);
	printf("raw stderr_buf:\n%s\n", stderr_buf);

    /* get rid of indent */
    for(i=0; stdout_buf[i]!='\0'; ++i) {
        if(0 == strncmp(stdout_buf+i, "\x0A\t", 2)) {
            memcpy(stdout_buf+i, "\x0a ", 2);
            i += 1;
        }
        else if(0 == strncmp(stdout_buf+i, "\x0d\x0a\t", 3)) {
            memcpy(stdout_buf+i, "\x0d\x0a ", 3);
            i += 2;
        }
    }

    gui->asmBuf->text(stdout_buf);

    rc = 0;
    cleanup:
    if(srcText) {
        free(srcText);
        srcText = NULL;
    }
    return;
}

/*****************************************************************************/
/* GUI CALLBACK STUFF */
/*****************************************************************************/

/* recompile request from GUI with 0 args */
void
recompile(void)
{
    printf("recompile request\n");
    compile_forced = true;
}

/* callback when the source code is changed
    (normal callback won't trigger during text change) */
void
onSourceModified(int pos, int nInserted, int nDeleted, int nRestyled,
    const char * deletedText, void *cbArg)
{
    printf("%s()\n", __func__);
    compile_requested = true;
}

void
onButtonC(void)
{
    gui->btnCPP->clear();
    compile_forced = true;
}

void
onButtonCPP(void)
{
    gui->btnC->clear();
    compile_forced = true;
}

void
onGuiFinished(ClabGui *gui_)
{
    printf("%s()\n", __func__);
    int rc = -1;

    gui = gui_;

    FILE *fp;

    /* alloc */
    stdout_buf = (char *)malloc(STREAM_BUF_SIZE);
    stderr_buf = (char *)malloc(STREAM_BUF_SIZE);
    if(!stdout_buf || !stderr_buf) {
        printf("ERROR: malloc()\n");
        goto cleanup;
    }

    /* generate the temp file paths */
    if(gen_tmp_file("clabXXXXXX", ePath, &fp)) {
        printf("ERROR: get_tmp_file()\n");
        goto cleanup;
    }
    fclose(fp);

    strcpy(cPath, ePath);
    strcat(cPath, ".c");
    strcpy(cppPath, ePath);
    strcat(cppPath, ".cpp");
    printf("ePath: %s\n", ePath);
    printf("cPath: %s\n", cPath);
    printf("cppPath: %s\n", cppPath);

    /* default GUI states */
    gui->btnWrap->set();
    gui->btnStdout->set();
    gui->btnStderr->set();
    gui->btnC->clear();
    gui->btnCPP->set();

    /* default input choices */
    gui->icOptimization->add("-O0");
    gui->icOptimization->add("-O1");
    gui->icOptimization->add("-O2");
    gui->icOptimization->add("-O3");
    gui->icOptimization->add("-Ofast");
    gui->icOptimization->add("-Os");
    gui->icOptimization->add("-Oz");
    gui->icOptimization->add("-O");
    gui->icOptimization->add("-O4");
    gui->icOptimization->add("");
    gui->icOptimization->value(0);
    gui->icCompiler->add("clang++ (in path)");
    gui->icCompiler->add("clang (in path)");
    gui->icCompiler->value(0);
    gui->icDebug->add("-g");
    gui->icDebug->add("-fstandalone-debug");
    gui->icDebug->add("-fno-standalone-debug");
    gui->icDebug->add("");
    gui->icDebug->value(0);

    /* initial source */
    gui->srcBuf->text(
        "// example source code\n"
        "#include <stdio.h>\n"
        "\n"
        "int main(int ac, char **av)\n"
        "{\n"
        "\tprintf(\"Hello, world!\\n\");\n"
        "\treturn 0;\n"
        "}\n"
    );

    gui->srcBuf->text(
        "#include <stdio.h>\n"
        "#include <memory>\n"
        "\n"
        "// what's a class doing here?\n"
        "class Foo\n"
        "{\n"
        "    private:\n"
        "        int bar;\n"
        "        int baz;\n"
        "\n"
        "    public:\n"
        "        Foo(int a, int b) { bar=a; baz=b; }\n"
        "        Foo() {}\n"
        "	void speak(void) { printf(\"I'm here!\\n\"); }\n"
        "};\n"
        "\n"
        "// what's a main doing here?\n"
        "int main(int ac, char **av)\n"
        "{\n"
        "	std::unique_ptr<Foo> pfoo(new Foo(3,4));\n"
        "	pfoo->speak();\n"
        "	printf(\"Hello, world!\\n\");\n"
        "	return 0;\n"
        "}\n"
    );

    compile_forced = true;

    rc = 0;
    cleanup:
    return;
}

void
onIdle(void *data)
{
    compile();
}

void
onExit(void)
{
    printf("onExit()\n");
    if(cPath[0]) remove(cPath);
    if(cPath[0]) remove(cppPath);
    if(ePath[0]) remove(ePath);
    if(stdout_buf) free(stdout_buf);
    if(stderr_buf) free(stderr_buf);
}
