#!/usr/bin/python

import sys
import binascii
from struct import unpack;
from hlab_taglib import *

# Constants for the cmd field of all load commands, the type
LC_REQ_DYLD = 0x80000000
LC_SEGMENT = 0x1
LC_SYMTAB = 0x2
LC_SYMSEG = 0x3
LC_THREAD = 0x4
LC_UNIXTHREAD = 0x5
LC_LOADFVMLIB = 0x6
LC_IDFVMLIB = 0x7
LC_IDENT = 0x8
LC_FVMFILE = 0x9
LC_PREPAGE = 0xa	
LC_DYSYMTAB = 0xb
LC_LOAD_DYLIB = 0xc
LC_ID_DYLIB = 0xd
LC_LOAD_DYLINKER = 0xe
LC_ID_DYLINKER = 0xf
LC_PREBOUND_DYLIB = 0x10
LC_ROUTINES = 0x11
LC_SUB_FRAMEWORK = 0x12
LC_SUB_UMBRELLA = 0x13
LC_SUB_CLIENT = 0x14
LC_SUB_LIBRARY  = 0x15
LC_TWOLEVEL_HINTS = 0x16
LC_PREBIND_CKSUM  = 0x17
LC_LOAD_WEAK_DYLIB = (0x18 | LC_REQ_DYLD)
LC_SEGMENT_64 = 0x19
LC_ROUTINES_64 = 0x1a
LC_UUID = 0x1b
LC_RPATH = (0x1c | LC_REQ_DYLD)   
LC_CODE_SIGNATURE = 0x1d
LC_SEGMENT_SPLIT_INFO = 0x1e
LC_REEXPORT_DYLIB = (0x1f | LC_REQ_DYLD)
LC_LAZY_LOAD_DYLIB = 0x20
LC_ENCRYPTION_INFO = 0x21
LC_DYLD_INFO  = 0x22
LC_DYLD_INFO_ONLY = (0x22|LC_REQ_DYLD)
LC_LOAD_UPWARD_DYLIB = (0x23 | LC_REQ_DYLD)
LC_VERSION_MIN_MACOSX = 0x24  
LC_VERSION_MIN_IPHONEOS = 0x25
LC_FUNCTION_STARTS = 0x26
LC_DYLD_ENVIRONMENT = 0x27
LC_MAIN = (0x28|LC_REQ_DYLD)
LC_DATA_IN_CODE = 0x29
LC_SOURCE_VERSION = 0x2A
LC_DYLIB_CODE_SIGN_DRS = 0x2B
LC_ENCRYPTION_INFO_64 = 0x2C
LC_LINKER_OPTION = 0x2D
LC_LINKER_OPTIMIZATION_HINT = 0x2E

def isMachO64(fp):
	tmp = fp.tell()
	fp.seek(0)
	if fp.read(4) != "\xcf\xfa\xed\xfe":
		return False
	if fp.read(4) != "\x07\x00\x00\x01":
		return False
	if fp.read(4) != "\x03\x00\x00\x80":
		return False
	fp.seek(tmp)
	return True

###############################################################################
# API that taggers must implement
###############################################################################

def tagTest(fpathIn):
	fp = open(fpathIn, "rb")
	result = isMachO64(fp)
	fp.close()
	return result

def tagReally(fpathIn, fpathOut):
	fp = open(fpathIn, "rb")
	assert(isMachO64(fp))

	# we want to be keep the convenience of writing tags to stdout with print()
	stdoutOld = None
	if fpathOut:
		stdoutOld = sys.stdout
		sys.stdout = open(fpathOut, "w")

	tag(fp, 4+4+4+4+4+4+4+4, "mach_header_64", 1)
	tagUint32(fp, "magic")
	tagUint32(fp, "cputype")
	tagUint32(fp, "cpusubtype")
	tagUint32(fp, "filetype")
	ncmds = tagUint32(fp, "ncmds")
	tagUint32(fp, "sizeofcmds")
	tagUint32(fp, "flags")
	tagUint32(fp, "reserved")
	
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
		
		elif cmd == LC_LOAD_DYLIB:
			# parse the dylib
			tag(fp, 16, "dylib", 1)
			lc_str = tagUint32(fp, "lc_str")
			tagUint32(fp, "timestamp")
			tagUint32(fp, "current_version")
			tagUint32(fp, "compatibility_version")
	
			# parse the string after the dylib (but before the end of the command)
			fp.seek(oCmd + lc_str)
			path = fp.read(cmdSize - lc_str).rstrip('\0')
	
			print '[0x%X,0x%X) 0x0 dylib_command \"%s\"' % \
				(oCmd, oCmd+cmdSize, path)
	
		elif cmd == LC_LOAD_DYLINKER:
			lc_str = tagUint32(fp, "lc_str")
			# parse the string after the dylinker_command (but before the end of the command)
			fp.seek(oCmd + lc_str)
			path = fp.read(cmdSize - lc_str).rstrip('\0')
	
			print '[0x%X,0x%X) 0x0 dylnker_command \"%s\"' % \
				(oCmd, oCmd+cmdSize, path)
	
		elif (cmd == LC_DYLD_INFO) or (cmd == LC_DYLD_INFO_ONLY):
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
	
		elif cmd == LC_SYMTAB:
			tagUint32(fp, "symoff")
			tagUint32(fp, "nsyms")
			tagUint32(fp, "stroff")
			tagUint32(fp, "strsize")
			print '[0x%X,0x%X) 0x0 symtab_command' % \
				(oCmd, fp.tell())
	
		elif cmd == LC_UUID:
			uuid = tag(fp, 16, "uuid")
			print '[0x%X,0x%X) 0x0 uuid_command "%s"' % \
				(oCmd, oCmd+cmdSize, binascii.hexlify(uuid))
	
		elif cmd == LC_VERSION_MIN_MACOSX or cmd == LC_VERSION_MIN_IPHONEOS:
			version = tagUint32(fp, "version")
			x = (version & 0xFFFF0000) >> 16
			y = (version & 0x0000FF00) >> 8
			z = (version & 0x000000FF) >> 0
			strVersion = '%d.%d.%d' % (x,y,z)
			sdk = tagUint32(fp, "sdk")
			x = (sdk & 0xFFFF0000) >> 16
			y = (sdk & 0x0000FF00) >> 8
			z = (sdk & 0x000000FF) >> 0
			strSdk = '%d.%d.%d' % (x,y,z)
			print '[0x%X,0x%X) 0x0 version_min_command ver=%s sdk=%s' % \
				(oCmd, oCmd+cmdSize, strVersion, strSdk)
			
		elif cmd == LC_SOURCE_VERSION:
			version = tagUint64(fp, "version")
			a = (0xFFFFFF0000000000 & version) >> 40
			b = (0x000000FFC0000000 & version) >> 30
			c = (0x000000003FF00000 & version) >> 20
			d = (0x00000000000FFC00 & version) >> 10
			e = (0x00000000000003FF & version) >> 0
			print '[0x%X,0x%X) 0x0 source_version_command %s.%s.%s.%s.%s' % \
				(oCmd, oCmd+cmdSize, str(a), str(b), str(c), str(d), str(e))
	
		elif cmd in [LC_CODE_SIGNATURE, LC_SEGMENT_SPLIT_INFO, LC_FUNCTION_STARTS, LC_DATA_IN_CODE]:
			extra = {LC_CODE_SIGNATURE:'signature', LC_SEGMENT_SPLIT_INFO:'splitinfo',
				LC_FUNCTION_STARTS:'function_starts', LC_DATA_IN_CODE:'data_in_code'}[cmd]
	
			tagUint32(fp, "dataoffs")
			tagUint32(fp, "datasize")
			print '[0x%X,0x%X) 0x0 linkedit_data_command %s' % \
				(oCmd, oCmd+cmdSize, extra)
	
		elif cmd == LC_MAIN:
			entrypoint = tagUint32(fp, "entryoff")
			tagUint32(fp, "stacksize")
			print '[0x%X,0x%X) 0x0 entry_point_command (main @0x%08X)' % \
				(oCmd, oCmd+cmdSize, entrypoint)
			if fp.tell() < (oCmd+cmdSize):
				tag(fp, oCmd+cmdSize-fp.tell(), "padding")
	
		#elif cmd == LC_DATA_IN_CODE:
	
		elif cmd == LC_DYSYMTAB:
			tagUint32(fp, "ilocalsym")
			tagUint32(fp, "nlocalsym")
			tagUint32(fp, "iextdefsym")
			tagUint32(fp, "nextdefsym")
			tagUint32(fp, "iundefsym")
			tagUint32(fp, "nundefsym")
			tagUint32(fp, "tocoff")
			tagUint32(fp, "ntoc")
			tagUint32(fp, "modtaboff")
			tagUint32(fp, "nmodtab")
			tagUint32(fp, "extrefsymoff")
			tagUint32(fp, "nextrefsyms")
			tagUint32(fp, "indirectsymoff")
			tagUint32(fp, "nindirectsyms")
			tagUint32(fp, "extreloff")
			tagUint32(fp, "nextrel")
			tagUint32(fp, "locreloff")
			tagUint32(fp, "nlocrel")
			print '[0x%X,0x%X) 0x0 dysymtab_command' % \
				(oCmd, fp.tell())
	
		else:
			print '[0x%X,0x%X) 0x0 unknown command 0x%X (%d)' % \
				(oCmd, oCmd+cmdSize, cmd, cmd)
			fp.seek(oCmd+cmdSize)
			
	fp.close()

	# undo our output redirection
	if stdoutOld:
		sys.stdout.close()
		sys.stdout = stdoutOld

###############################################################################
# "main"
###############################################################################

if __name__ == '__main__':
	fpathIn = None
	fpathOut = None

	assert len(sys.argv) > 1

	fpathIn = sys.argv[1]
	if sys.argv[2:]:
		fpathOut = sys.argv[2]

	tagReally(fpathIn, fpathOut)

