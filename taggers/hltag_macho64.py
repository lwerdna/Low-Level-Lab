#!/usr/bin/env python

# references (if you have it):
# source:
#   /usr/include/mach-o/loader.h and others
#   <llvm>/include/llvm/Support/MachO.h
# programs:
#   otool -fahl <file>

import sys
import binascii
from struct import unpack;
from hlab_taglib import *

MH_MAGIC = 0xFEEDFACE
MH_CIGAM = 0xCEFAEDFE
MH_MAGIC_64 = 0xFEEDFACF
MH_CIGAM_64 = 0xCFFAEDFE
FAT_MAGIC = 0xCAFEBABE
FAT_CIGAM = 0xBEBAFECA
lookup_magic = {MH_MAGIC:'MH_MAGIC', MH_CIGAM:'MH_CIGAM', MH_MAGIC_64:'MH_MAGIC_64',
	MH_CIGAM_64:'MH_CIGAM_64', FAT_MAGIC:'FAT_MAGIC', FAT_CIGAM:'FAT_CIGAM'}

MH_OBJECT = 0x1
MH_EXECUTE = 0x2
MH_FVMLIB = 0x3
MH_CORE = 0x4
MH_PRELOAD = 0x5
MH_DYLIB = 0x6
MH_DYLINKER = 0x7
MH_BUNDLE = 0x8
MH_DYLIB_STUB = 0x9
MH_DSYM = 0xA
MH_KEXT_BUNDLE = 0xB
lookup_filetype = {MH_OBJECT:'MH_OBJECT', MH_EXECUTE:'MH_EXECUTE', MH_FVMLIB:'MH_FVMLIB',
	MH_CORE:'MH_CORE', MH_PRELOAD:'MH_PRELOAD', MH_DYLIB:'MH_DYLIB', MH_DYLINKER:'MH_DYLINKER',
	MH_BUNDLE:'MH_BUNDLE', MH_DYLIB_STUB:'MH_DYLIB_STUB', MH_DSYM:'MH_DSYM', 
	MH_KEXT_BUNDLE:'MH_KEXT_BUNDLE'}

# CPU type constants
CPU_ARCH_ABI64 = 0x01000000
CPU_TYPE_ANY = 0xFFFFFFFF
CPU_TYPE_VAX = 1
CPU_TYPE_MC680x0 = 6
CPU_TYPE_X86 = 7
CPU_TYPE_I386 = CPU_TYPE_X86
CPU_TYPE_X86_64 = CPU_TYPE_X86 | CPU_ARCH_ABI64
CPU_TYPE_MIPS = 8
CPU_TYPE_MC98000 = 10
CPU_TYPE_HPPA = 11
CPU_TYPE_ARM = 12
CPU_TYPE_MC88000 = 13
CPU_TYPE_SPARC = 14
CPU_TYPE_I860 = 15
CPU_TYPE_ALPHA = 16
CPU_TYPE_POWERPC = 18
CPU_TYPE_POWERPC64 = CPU_TYPE_POWERPC | CPU_ARCH_ABI64
lookup_cputype = {CPU_TYPE_ANY:'CPU_TYPE_ANY', CPU_TYPE_VAX:'CPU_TYPE_VAX', 
	CPU_TYPE_MC680x0:'CPU_TYPE_MC680x0', CPU_TYPE_X86:'CPU_TYPE_X86', CPU_TYPE_I386:'CPU_TYPE_I386', 
	CPU_TYPE_X86_64:'CPU_TYPE_X86_64', CPU_TYPE_MIPS:'CPU_TYPE_MIPS', CPU_TYPE_MC98000:'CPU_TYPE_MC98000', 
	CPU_TYPE_HPPA:'CPU_TYPE_HPPA', CPU_TYPE_ARM:'CPU_TYPE_ARM', CPU_TYPE_MC88000:'CPU_TYPE_MC88000',
	CPU_TYPE_SPARC:'CPU_TYPE_SPARC', CPU_TYPE_I860:'CPU_TYPE_I860', CPU_TYPE_ALPHA:'CPU_TYPE_ALPHA',
	CPU_TYPE_POWERPC:'CPU_TYPE_POWERPC', CPU_TYPE_POWERPC64:'CPU_TYPE_POWERPC64'
}

# CPU subtype constants
CPU_SUBTYPE_MASK = 0xFF000000
CPU_SUBTYPE_LIB64 = 0x80000000
CPU_SUBTYPE_MULTIPLE = 0xFFFFFFFF
CPU_SUBTYPE_I386_ALL = 3
CPU_SUBTYPE_386 = 3
CPU_SUBTYPE_486 = 4
CPU_SUBTYPE_486SX = 0x84
CPU_SUBTYPE_586 = 5
CPU_SUBTYPE_PENT = CPU_SUBTYPE_586
CPU_SUBTYPE_PENTPRO = 0x16
CPU_SUBTYPE_PENTII_M3 = 0x36
CPU_SUBTYPE_PENTII_M5 = 0x56
CPU_SUBTYPE_CELERON = 0x67
CPU_SUBTYPE_CELERON_MOBILE = 0x77
CPU_SUBTYPE_PENTIUM_3 = 0x08
CPU_SUBTYPE_PENTIUM_3_M = 0x18
CPU_SUBTYPE_PENTIUM_3_XEON = 0x28
CPU_SUBTYPE_PENTIUM_M = 0x09
CPU_SUBTYPE_PENTIUM_4 = 0x0a
CPU_SUBTYPE_PENTIUM_4_M = 0x1a
CPU_SUBTYPE_ITANIUM = 0x0b
CPU_SUBTYPE_ITANIUM_2 = 0x1b
CPU_SUBTYPE_XEON = 0x0c
CPU_SUBTYPE_XEON_MP = 0x1c
lookup_cpusubtype_capabilities = { 0:'None', CPU_SUBTYPE_LIB64:'CPU_SUBTYPE_LIB64' }
lookup_cpusubtype = { CPU_SUBTYPE_I386_ALL:'CPU_SUBTYPE_I386_ALL', CPU_SUBTYPE_386:'CPU_SUBTYPE_386', CPU_SUBTYPE_486:'CPU_SUBTYPE_486',
	CPU_SUBTYPE_486SX:'CPU_SUBTYPE_486SX', CPU_SUBTYPE_586:'CPU_SUBTYPE_586', CPU_SUBTYPE_PENT:'CPU_SUBTYPE_PENT',
	CPU_SUBTYPE_PENTPRO:'CPU_SUBTYPE_PENTPRO', CPU_SUBTYPE_PENTII_M3:'CPU_SUBTYPE_PENTII_M3', CPU_SUBTYPE_PENTII_M5:'CPU_SUBTYPE_PENTII_M5',
	CPU_SUBTYPE_CELERON:'CPU_SUBTYPE_CELERON', CPU_SUBTYPE_CELERON_MOBILE:'CPU_SUBTYPE_CELERON_MOBILE', CPU_SUBTYPE_PENTIUM_3:'CPU_SUBTYPE_PENTIUM_3',
	CPU_SUBTYPE_PENTIUM_3_M:'CPU_SUBTYPE_PENTIUM_3_M', CPU_SUBTYPE_PENTIUM_3_XEON:'CPU_SUBTYPE_PENTIUM_3_XEON', CPU_SUBTYPE_PENTIUM_M:'CPU_SUBTYPE_PENTIUM_M',
	CPU_SUBTYPE_PENTIUM_4:'CPU_SUBTYPE_PENTIUM_4', CPU_SUBTYPE_PENTIUM_4_M:'CPU_SUBTYPE_PENTIUM_4_M', CPU_SUBTYPE_ITANIUM:'CPU_SUBTYPE_ITANIUM',
	CPU_SUBTYPE_ITANIUM_2:'CPU_SUBTYPE_ITANIUM_2', CPU_SUBTYPE_XEON:'CPU_SUBTYPE_XEON', CPU_SUBTYPE_XEON_MP:'CPU_SUBTYPE_XEON_MP',
}

MH_NOUNDEFS = 0x00000001
MH_INCRLINK = 0x00000002
MH_DYLDLINK = 0x00000004
MH_BINDATLOAD = 0x00000008
MH_PREBOUND = 0x00000010
MH_SPLIT_SEGS = 0x00000020
MH_LAZY_INIT = 0x00000040
MH_TWOLEVEL = 0x00000080
MH_FORCE_FLAT = 0x00000100
MH_NOMULTIDEFS = 0x00000200
MH_NOFIXPREBINDING = 0x00000400
MH_PREBINDABLE = 0x00000800
MH_ALLMODSBOUND = 0x00001000
MH_SUBSECTIONS_VIA_SYMBOLS = 0x00002000
MH_CANONICAL = 0x00004000
MH_WEAK_DEFINES = 0x00008000
MH_BINDS_TO_WEAK = 0x00010000
MH_ALLOW_STACK_EXECUTION = 0x00020000
MH_ROOT_SAFE = 0x00040000
MH_SETUID_SAFE = 0x00080000
MH_NO_REEXPORTED_DYLIBS = 0x00100000
MH_PIE = 0x00200000
MH_DEAD_STRIPPABLE_DYLIB = 0x00400000
MH_HAS_TLV_DESCRIPTORS = 0x00800000
MH_NO_HEAP_EXECUTION = 0x01000000
MH_APP_EXTENSION_SAFE = 0x02000000

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

###############################################################################
# "main"
###############################################################################

if __name__ == '__main__':
	assert len(sys.argv) == 2

	fp = open(sys.argv[1], "rb")

	tmp = fp.tell()
	fp.seek(0)
	if uint32(fp) != MH_MAGIC_64: # magic
		sys.exit(-1);
	if uint32(fp) != CPU_TYPE_X86_64: # cputype
		sys.exit(-1);
	if (uint32(fp) & 0xFF) != CPU_SUBTYPE_386:
		sys.exit(-1);
	fp.seek(tmp)

	tag(fp, 4+4+4+4+4+4+4+4, "mach_header_64", 1)
	# magic
	magic = uint32(fp, True)
	tag(fp, 4, "magic=%08X (%s)" % (magic, lookup_magic[magic]))
	# cputype
	cputype = uint32(fp, True)
	tag(fp, 4, "cputype=%08X (%s)" % (cputype, lookup_cputype[cputype]))
	# cpu subtype
	a = uint32(fp, True)
	subtype = a & 0xFF
	capabilities = a & CPU_SUBTYPE_MASK
	b = lookup_cpusubtype[subtype]
	if capabilities:
		b = '%s|%s' % (lookup_cpusubtype_capabilities[capabilities], b)
	tag(fp, 4, "cpusubtype=%08X (%s)" % (a, b))
	# filetype
	filetype = uint32(fp, True)
	tag(fp, 4, "filetype=%08X (%s)" % (filetype, lookup_filetype[filetype]))
	# etc....
	ncmds = tagUint32(fp, "ncmds")
	tagUint32(fp, "sizeofcmds") # some of all cmdSize to follow
	tagUint32(fp, "flags")
	tagUint32(fp, "reserved")
	
	for i in range(ncmds):
		oCmd = fp.tell()
		cmd = tagUint32(fp, "cmd")
		cmdSize = tagUint32(fp, "cmdsize") # includes cmd,cmdsize
		
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
	sys.exit(0);
