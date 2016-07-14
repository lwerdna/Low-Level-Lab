#include <stdio.h>

#include <FL/Fl.H>
#include <FL/Fl_Text_Buffer.H>

extern "C" {
#include <autils/filesys.h>
}

void
onSourceModified(int pos, int nInserted, int nDeleted, int nRestyled,
    const char * deletedText, void *cbArg)
{
    char cPath[128];

    char *cSource = NULL;

    printf("onSourceModified!\n");

    Fl_Text_Buffer *buf = (Fl_Text_Buffer *)cbArg;

    printf("buffer is %d bytes\n", buf->length());

    cSource = buf->text();
    printf("got source:\n%s\n", cSource);

    FILE *fp;
    if(gen_tmp_file("clabXX.c", cPath, &fp)) {
        printf("ERROR: get_tmp_file()\n");
    }

    printf("got temp path: %s\n", cPath);
    fclose(fp);
}

