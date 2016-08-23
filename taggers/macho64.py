#!/usr/bin/python

import sys

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

fp.close()
