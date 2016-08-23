#!/usr/bin/python

import sys
from struct import unpack;
from taglib import *

LC_SEGMENT_64 = 0x19
LC_DYLD_INFO = 0x22

assert len(sys.argv) > 1
fp = open(sys.argv[1], "rb")

tag(fp, 4+4+4+4+4+4+4+4, "mach_header_64", 1)
tag(fp, 4, "magic")
tag(fp, 4, "cputype")
tag(fp, 4, "cpusubtype")
tag(fp, 4, "filetype")
ncmds = uint32(fp, 1)
tag(fp, 4, "ncmds")
sizeofcmds = uint32(fp, 1)
tag(fp, 4, "sizeofcmds")
tag(fp, 4, "flags")
tag(fp, 4, "reserved")

for i in range(ncmds):
    oCmd = fp.tell()
    cmd = tagUint32(fp, "cmd")
    cmdSize = tagUint32(fp, "cmdsize")
    
    if cmd == LC_SEGMENT_64:
        segname = tagString(fp, 16, "segname")
        tagUint64(fp, "vmaddr")
        tagUint64(fp, "vmsize")
        tagUint64(fp, "fileoff")
        tagUint64(fp, "filesize")
        tagUint32(fp, "maxprot")
        tagUint32(fp, "initprot")
        nsects = tagUint32(fp, "nsects")
        flags = tagUint32(fp, "flags")
       
        for j in range(nsects):
            oScn = fp.tell()
            sectname = tagString(fp, 16, "sectname")
            segname = tagString(fp, 16, "segname")
            tagUint64(fp, "addr")
            tagUint64(fp, "size")
            tagUint32(fp, "offset")
            tagUint32(fp, "align")
            tagUint32(fp, "reloff")
            tagUint32(fp, "nreloc")
            tagUint32(fp, "flags")
            tagUint32(fp, "reserved1")
            tagUint32(fp, "reserved2")
            tagUint32(fp, "reserved3")
            print '[0x%X,0x%X) 0x0 section_64 \"%s\" %d/%d' % \
                (oScn, fp.tell(), sectname, j+1, nsects)

        print '[0x%X,0x%X) 0x0 segment_command_64 \"%s\"' % \
            (oCmd, fp.tell(), segname)

    elif cmd == LC_DYLD_INFO:
        tagUint32(fp, "cmd")
        tagUint32(fp, "cmdsize")
        tagUint32(fp, "rebase_off")
        tagUint32(fp, "rebase_size")
        tagUint32(fp, "bind_off")
        tagUint32(fp, "bind_size")
        tagUint32(fp, "weak_bind_off")
        tagUint32(fp, "weak_bind_size")
        tagUint32(fp, "lazy_bind_off")
        tagUint32(fp, "lazy_bind_size")
        tagUint32(fp, "export_off")
        tagUint32(fp, "export_size")
        print '[0x%X,0x%X) 0x0 dyld_info_command' % \
            (oCmd, fp.tell())

fp.close()
