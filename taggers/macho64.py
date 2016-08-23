#!/usr/bin/python

import sys
from struct import unpack;
from taglib import *

assert len(sys.argv) > 1
fp = open(sys.argv[1], "rb")

tag(fp, 4+4+4+4+4+4+4+4, "mach_header_64", 1);
tag(fp, 4, "magic");
tag(fp, 4, "cputype");
tag(fp, 4, "cpusubtype");
tag(fp, 4, "filetype");
ncmds = uint32(fp, 1);
tag(fp, 4, "ncmds");
sizeofcmds = uint32(fp, 1)
tag(fp, 4, "sizeofcmds");
tag(fp, 4, "flags");
tag(fp, 4, "reserved");

for i in range(ncmds):
    tmp = fp.tell();
    tag(fp, 4, "cmd");
    tag(fp, 4, "cmdsize");
    segname = string(fp, 16, 1)
    tag(fp, 16, "segname \"%s\"" % segname);
    tag(fp, 4, "vmaddr");
    tag(fp, 4, "vmsize");
    tag(fp, 4, "fileoff");
    tag(fp, 4, "filesize");
    tag(fp, 4, "maxprot");
    tag(fp, 4, "initprot");
    tag(fp, 4, "nsects");
    tag(fp, 4, "flags");
    fp.seek(tmp);
    tag(fp, 4+4+16+8+8+8+8+4+4+4+4, "segment_command_64 \"%s\" %d/%d" % (segname, i+1, ncmds))

fp.close()
