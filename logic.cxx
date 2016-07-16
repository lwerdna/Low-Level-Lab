#include <time.h>
#include <stdio.h>

#include <FL/Fl.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Display.H>

#include "Gui.h"


extern "C" {
#include <autils/crc.h>
#include <autils/filesys.h>
#include <autils/subprocess.h>
}

Gui *gui = NULL;

/* recompile state vars */
bool compile_forced = false;
bool compile_requested = false;
int length_last_compile = 0;
time_t time_last_compile = 0;
uint32_t crc_last_compile = 0;

/* temp files used for source, exec */
char cPath[128], ePath[128];

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
    int rc = -1, rc_child, i;
    int n_args;
    char stdout_buf[2048];
    char stderr_buf[2048];
    char *argv[6];
    char *cSource = NULL;
    FILE *fp;
    char ca_clang[] = "clang";
    char ca_otool[] = "otool";
    char ca_dash_t[] = "-t";
    char ca_dash_V[] = "-V";
    char ca_dash_v[] = "-v";
    char ca_dash_X[] = "-X";
    char ca_dash_o[] = "-o";

    /* compile timing vars */
    time_t time_now;
    int length_now;
    uint32_t crc_now;

    Fl_Text_Buffer *srcBuf = gui->srcBuf;
    Fl_Text_Buffer *asmBuf = gui->asmBuf;
    Fl_Text_Buffer *outBuf = gui->outBuf;

    cSource = srcBuf->text();

    if(!compile_forced && !compile_requested) goto cleanup;
    
    if(!compile_forced && compile_requested) {
        /* skip if we compiled within the last second */
        time(&time_now);
        if(difftime(time_now, time_last_compile) < 1) {
            /* just too soon, remain requested */
            goto cleanup;
        }
        else {
            /* skip if the text is unchanged */
            length_now = srcBuf->length();
            if(length_last_compile == length_now) {
                crc_now = crc32(0, cSource, srcBuf->length());
                if(crc_now == crc_last_compile) {
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

    fp = fopen(cPath, "w");
    fwrite(cSource, 1, srcBuf->length(), fp);
    fclose(fp);

    memset(stdout_buf, '\0', sizeof(stdout_buf));
    memset(stderr_buf, '\0', sizeof(stderr_buf));

    /* form compiler command line, invoke compiler */
    i=0;
    argv[i++] = ca_clang;
    //argv[i++] = "-v";
    argv[i++] = cPath;
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

    if(0 != launch(ca_clang, argv, &rc_child, stdout_buf, sizeof(stdout_buf),
        stderr_buf, sizeof(stderr_buf)))
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

    /* disassemble shit */
    i=0;
    argv[i++] = ca_otool;
    argv[i++] = ca_dash_t;
    argv[i++] = ePath;
    argv[i++] = ca_dash_V;
    argv[i++] = ca_dash_X;
    argv[i++] = NULL;

    memset(stdout_buf, '\0', sizeof(stdout_buf));
    memset(stderr_buf, '\0', sizeof(stderr_buf));

    {
        printf("launching ");
        for(i=0; argv[i]!=NULL; ++i)
            printf("%s ", argv[i]);
        printf("\n");
    }
    if(0 != launch(ca_otool, argv, &rc_child, stdout_buf, sizeof(stdout_buf),
        stderr_buf, sizeof(stderr_buf)))
    {
        printf("ERROR: launch()");
        goto cleanup;
    }

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
    if(cSource) {
        free(cSource);
        cSource = NULL;
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
    compile_requested = true;
}

void
onGuiFinished(Gui *gui_)
{
    int rc = -1;

    printf("onGuiFinished\n");

    gui = gui_;

    FILE *fp;

    /* generate the temp file paths */
    if(gen_tmp_file("clabXXXXXX", ePath, &fp)) {
        printf("ERROR: get_tmp_file()\n");
        goto cleanup;
    }
    fclose(fp);

    strcpy(cPath, ePath);
    strcat(cPath, ".c");
    printf("ePath: %s\n", ePath);
    printf("cPath: %s\n", cPath);

    /* default GUI states */
    gui->btnWrap->set();
    gui->btnStdout->set();
    gui->btnStderr->set();

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
    gui->icCompiler->add("clang (in path)");
    gui->icCompiler->value(0);
    gui->icDebug->add("-g");
    gui->icDebug->add("-fstandalone-debug");
    gui->icDebug->add("-fno-standalone-debug");
    gui->icDebug->add("");
    gui->icDebug->value(0);

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
    if(ePath[0]) remove(ePath);
}
