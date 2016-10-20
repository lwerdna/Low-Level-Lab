#!/usr/bin/python

import sys
import struct
import binascii
from hlab_taglib import *

EI_NIDENT = 16
ELFMAG = '\177ELF'

# section header type
SHT_NULL = 0
SHT_PROGBITS = 1
SHT_SYMTAB = 2
SHT_STRTAB = 3
SHT_RELA = 4
SHT_HASH = 5
SHT_DYNAMIC = 6
SHT_NOTE = 7
SHT_NOBITS = 8
SHT_REL = 9
SHT_SHLIB = 10
SHT_DYNSYM = 11
SHT_NUM = 12
SHT_LOPROC = 0x70000000
SHT_HIPROC = 0x7fffffff
SHT_LOUSER = 0x80000000
SHT_HIUSER = 0xffffffff

SHT_NULL = 0
SHT_PROGBITS = 1
SHT_SYMTAB = 2
SHT_STRTAB = 3
SHT_RELA = 4
SHT_HASH = 5
SHT_DYNAMIC = 6
SHT_NOTE = 7
SHT_NOBITS = 8
SHT_REL = 9
SHT_SHLIB = 10
SHT_DYNSYM = 11
SHT_INIT_ARRAY = 14
SHT_FINI_ARRAY = 15
SHT_PREINIT_ARRAY = 16
SHT_GROUP = 17
SHT_SYMTAB_SHNDX = 18
SHT_NUM = 19
SHT_GNU_ATTRIBUTES = 0x6ffffff5
SHT_GNU_HASH = 0x6ffffff6
SHT_GNU_LIBLIST = 0x6ffffff7
SHT_CHECKSUM = 0x6ffffff8
SHT_SUNW_move = 0x6ffffffa
SHT_SUNW_COMDAT = 0x6ffffffb
SHT_SUNW_syminfo = 0x6ffffffc
SHT_GNU_verdef = 0x6ffffffd
SHT_GNU_verneed = 0x6ffffffe
SHT_GNU_versym = 0x6fffffff
def sh_type_tostr(a):
	lookup = { 
		SHT_NULL:'NULL', SHT_PROGBITS:'PROGBITS', SHT_SYMTAB:'SYMTAB',
		SHT_STRTAB:'STRTAB', SHT_RELA:'RELA', SHT_HASH:'HASH',
		SHT_DYNAMIC:'DYNAMIC', SHT_NOTE:'NOTE', SHT_NOBITS:'NOBITS',
		SHT_REL:'REL', SHT_SHLIB:'SHLIB', SHT_DYNSYM:'DYNSYM',
		SHT_INIT_ARRAY:'INIT_ARRAY', SHT_FINI_ARRAY:'FINI_ARRAY', SHT_PREINIT_ARRAY:'PREINIT_ARRAY',
		SHT_GROUP:'GROUP', SHT_SYMTAB_SHNDX:'SYMTAB_SHNDX', SHT_NUM:'NUM',
		SHT_GNU_ATTRIBUTES:'GNU_ATTRIBUTES', SHT_GNU_HASH:'GNU_HASH', SHT_GNU_LIBLIST:'GNU_LIBLIST',
		SHT_CHECKSUM:'CHECKSUM', SHT_SUNW_move:'SUNW_move', SHT_SUNW_COMDAT:'SUNW_COMDAT',
		SHT_SUNW_syminfo:'SUNW_syminfo', SHT_GNU_verdef:'GNU_verdef', SHT_GNU_verneed:'GNU_verneed',
		SHT_GNU_versym:'GNU_versym'
	}
	if a in lookup:
		return lookup[a]
	return 'UNKNOWN'

SHF_WRITE = (1 << 0)
SHF_ALLOC = (1 << 1)
SHF_EXECINSTR = (1 << 2)
SHF_MERGE = (1 << 4)
SHF_STRINGS = (1 << 5)
SHF_INFO_LINK = (1 << 6)
SHF_LINK_ORDER = (1 << 7)
SHF_OS_NONCONFORMING = (1 << 8)
SHF_GROUP = (1 << 9)
SHF_TLS = (1 << 10)
def sh_flags_tostr(a):
	lookup = {
		SHF_WRITE:'WRITE', SHF_ALLOC:'ALLOC', SHF_EXECINSTR:'EXECINSTR',
		SHF_MERGE:'MERGE', SHF_STRINGS:'STRINGS', SHF_INFO_LINK:'INFO_LINK',
		SHF_LINK_ORDER:'LINK_ORDER', SHF_OS_NONCONFORMING:'OS_NONCONFORMING',
		SHF_GROUP:'GROUP', SHF_TLS:'TLS'
	}
	result = []
	for bit in lookup.keys():
		if a & bit:
			result.append(lookup[bit])
	if not result:
		if a==0:
			result = ['0']
		else:
			result = 'UNKNOWN'
	return '|'.join(result)

PT_NULL = 0
PT_LOAD = 1
PT_DYNAMIC = 2
PT_INTERP = 3
PT_NOTE = 4
PT_SHLIB = 5
PT_PHDR = 6
PT_TLS = 7
PT_LOOS = 0x60000000
PT_HIOS = 0x6fffffff
PT_LOPROC = 0x70000000
PT_HIPROC = 0x7fffffff
PT_GNU_EH_FRAME = 0x6474e550

###############################################################################
# helper classes
###############################################################################

def scnTypeToStr(sh_type):
	strType = 'UNKNOWN'

	if sh_type < 13:
		strType = ['NULL','PROGBITS','SYMTAB','STRTAB','RELA', \
		'HASH','DYNAMIC','NOTE','NOBITS','REL','SHLIB', \
		'DYNSYM','NUM'][sh_type]
	elif sh_type >= SHT_LOPROC and sh_type <= SHT_HIPROC:
		strType = 'PROC'
	elif sh_type >= SHT_LOUSER and sh_type <= SHT_HIUSER:
		strType = 'USER'

	return strType

def phdrTypeToStr(p_type):
	strType = 'UNKNOWN'

	lookup = ['NULL','LOAD','DYNAMIC','INTERP','NOTE', \
		'SHLIB','PHDR','TLS']

	if p_type < len(lookup):
		strType = lookup[p_type]
	elif p_type >= PT_LOOS and p_type <= PT_HIOS:
		strType = 'OS'
	elif p_type >= SHT_LOPROC and p_type <= SHT_HIPROC:
		strType = 'PROC'

	return strType

class StringTable:
	def __init__(self, FP, size):
		self.offset = FP.tell()
		self.size = size
		data = FP.read(size)
		self.table = unpack(('%d'%size)+'s', data)[0]
	def __getitem__(self, offset):
		end = offset;
		while self.table[end] != '\0':
			end += 1
		return self.table[offset:end]
	def replace_string(self, oldstr, newstr):
		offset = 0
		self.table.index(oldstr) # check existence, exception if not found
		self.table = self.table.replace(oldstr, newstr)
	def __str__(self):
		buff = 'offset'.rjust(12) + ' string' + "\n"
		for i in range(self.size):
			if self.table[i] != '\0':
				if i==0 or self.table[i-1] == '\0':
					buff += ('0x%X' % i).rjust(12) + ' ' + self[i] + "\n"
		return buff

	def toNode(self, node, extra=''):
		n = node.addNode("string table "+extra, self.offset, self.size)
		for i in range(self.size):
			if self.table[i] != '\0':
				if i==0 or self.table[i-1] == '\0':
					str_ = self[i]
					n.addNode('0x%X'%i + ' \"' + str_ + "\"", self.offset + i, len(str_)+1)

def isElf64(fp):
	tmp = fp.tell()
	fp.seek(0)
	if fp.read(4) != "\x7fELF": 
		#print "missing ELF header"
		return False
	if fp.read(1) != "\x02":
		#print "EI_CLASS should be 2"
		return False # e_ident[EI_CLASS] (64-bit)
	if fp.read(1) != "\x01": 
		#print "EI_DATA should be 1"
		return False # e_ident[EI_DATA]
	if fp.read(1) != "\x01": 
		#print "EI_VERSION should be 1"
		return False # e_ident[EI_VERSION]
	fp.seek(tmp)
	#print "returning True!"
	return True

###############################################################################
# API that taggers must implement
###############################################################################

def tagTest(fpathIn):
	fp = open(fpathIn, "rb")
	result = isElf64(fp)
	fp.close()
	return result

def tagReally(fpathIn, fpathOut):
	fp = open(fpathIn, "rb")
	assert(isElf64(fp))

	# we want to be keep the convenience of writing tags to stdout with print()
	stdoutOld = None
	if fpathOut:
		stdoutOld = sys.stdout
		sys.stdout = open(fpathOut, "w")

	tag(fp, 0x40, "elf64_hdr", 1)
	tmp = tag(fp, 4, "e_ident[0..4)")
	tmp = tagUint8(fp, "e_ident[EI_CLASS] (64-bit)")
	tmp = tagUint8(fp, "e_ident[EI_DATA] (little-end)")
	tmp = tagUint8(fp, "e_ident[EI_VERSION] (little-end)")
	tmp = tagUint8(fp, "e_ident[EI_OSABI]");
	tmp = tagUint8(fp, "e_ident[EI_ABIVERSION]");
	tmp = tag(fp, 7, "e_ident[EI_PAD]");
	tagUint16(fp, "e_type")
	tagUint16(fp, "e_machine")
	tagUint32(fp, "e_version")
	tagUint64(fp, "e_entry")
	e_phoff = tagUint64(fp, "e_phoff")
	e_shoff = tagUint64(fp, "e_shoff")
	tagUint32(fp, "e_flags")
	tagUint16(fp, "e_ehsize")
	tagUint16(fp, "e_phentsize")
	e_phnum = tagUint16(fp, "e_phnum")
	tagUint16(fp, "e_shentsize")
	e_shnum = tagUint16(fp, "e_shnum")
	e_shstrndx = tagUint16(fp, "e_shstrndx")
	
	# read the string table
	fp.seek(e_shoff + e_shstrndx*0x40)
	tmp = fp.tell()
	(a,b,c,d,sh_offset,sh_size) = struct.unpack('<IIQQQQ', fp.read(40));
	fp.seek(sh_offset)
	stringTable = StringTable(fp, sh_size)
	
	# read all section headers
	fp.seek(e_shoff)
	for i in range(e_shnum):
		oHdr = fp.tell()
		sh_name = tagUint32(fp, "sh_name")
		sh_type = uint32(fp, 1)
		tag(fp, 4, "sh_type=0x%X (%s)" % \
			(sh_type, sh_type_tostr(sh_type)))
		sh_flags = uint64(fp, 1)
		tag(fp, 8, "sh_flags=0x%X (%s)" % \
			(sh_flags, sh_flags_tostr(sh_flags)))
		tagUint64(fp, "sh_addr")
		sh_offset = tagUint64(fp, "sh_offset")
		sh_size = tagUint64(fp, "sh_size")
		tagUint32(fp, "sh_link")
		tagUint32(fp, "sh_info")
		tagUint64(fp, "sh_addralign")
		tagUint64(fp, "sh_entsize")
	
		strType = scnTypeToStr(sh_type)
	
		print '[0x%X,0x%X) 0x0 elf64_shdr "%s" %s' % \
			(oHdr, fp.tell(), stringTable[sh_name], strType)
	
		if(not sh_type in [SHT_NULL, SHT_NOBITS]):
			print '[0x%X,0x%X) 0x0 section "%s" contents' % \
				(sh_offset, sh_offset+sh_size, stringTable[sh_name])
	
	# read program headers
	fp.seek(e_phoff)
	for i in range(e_phnum):
		oHdr = fp.tell()
		p_type = tagUint32(fp, "p_type")
		tagUint32(fp, "p_flags")
		tagUint64(fp, "p_offset")
		tagUint64(fp, "p_vaddr")
		tagUint64(fp, "p_paddr")
		tagUint64(fp, "p_filesz")
		tagUint64(fp, "p_memsz")
		tagUint64(fp, "p_align")
	
		strType = phdrTypeToStr(p_type)
	
		print '[0x%X,0x%X) 0x0 elf64_phdr %d %s' % \
			(oHdr, fp.tell(), i, strType)
	
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






