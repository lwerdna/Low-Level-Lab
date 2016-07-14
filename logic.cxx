#include <stdio.h>

#include <FL/Fl.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Display.H>

#include "Gui.h"

extern "C" {
#include <autils/filesys.h>
#include <autils/subprocess.h>
}

void
onSourceModified(int pos, int nInserted, int nDeleted, int nRestyled,
    const char * deletedText, void *cbArg)
{
    int rc = -1, rc_child, i;
    char cPath[128], ePath[128];
    char stdout_buf[2048];
    char stderr_buf[2048];
    char *argv[6];

    char *cSource = NULL;

    Gui *gui = (Gui *)cbArg;
    Fl_Text_Buffer *srcBuf = gui->srcBuf;
    Fl_Text_Buffer *asmBuf = gui->asmBuf;
    Fl_Text_Buffer *outBuf = gui->outBuf;

    //printf("buffer is %d bytes\n", buf->length());
    cSource = srcBuf->text();
    //printf("got source:\n%s\n", cSource);

    FILE *fp;

    if(gen_tmp_file("clabXXXXXX", ePath, &fp)) {
        printf("ERROR: get_tmp_file()\n");
        goto cleanup;
    }
    fclose(fp);

    strcpy(cPath, ePath);
    strcat(cPath, ".c");
    printf("ePath: %s\n", ePath);
    printf("cPath: %s\n", cPath);

    fp = fopen(cPath, "w");
    fwrite(cSource, 1, srcBuf->length(), fp);
    fclose(fp);

    memset(stdout_buf, '\0', sizeof(stdout_buf));
    memset(stderr_buf, '\0', sizeof(stderr_buf));

    /* compile shit */
    i=0;
    argv[i++] = "clang";
    //argv[i++] = "-v";
    argv[i++] = cPath;
    argv[i++] = "-o";
    argv[i++] = ePath;
    argv[i++] = NULL;
    if(0 != launch("clang", argv, &rc_child, stdout_buf, sizeof(stdout_buf),
        stderr_buf, sizeof(stderr_buf)))
    {
        printf("ERROR: launch()");
        goto cleanup;
    }

    gui->outLog->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
    outBuf->text("STDOUT>\n");
    outBuf->append(stdout_buf);
    outBuf->append("STDERR>\n");
    outBuf->append(stderr_buf);

    rc = 0;
    cleanup:
    /* delete shit */
    if(cPath[0]) remove(cPath);
    if(ePath[0]) remove(ePath);
    return;
}

