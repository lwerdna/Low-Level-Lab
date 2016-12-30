#!/usr/bin/env python

import sys
import struct
import binascii
from hltag_lib import *

EI_NIDENT = 16
ELFMAG = '\x7FELF'

ELFLCASSNONE = 0
ELFCLASS32 = 1
ELFCLASS64 = 2
ELFCLASSNUM = 3

ELFDATANONE = 0
ELFDATA2LSB = 1
ELFDATA2MSB = 2
def ei_data_tostr(t):
	lookup = { ELFDATANONE:'NONE', ELFDATA2LSB:'LSB (little-end)', 
		ELFDATA2MSB:'MSB (big-end)' }

	if t in lookup:
		return lookup[t]
	else:
		return 'UNKNOWN'

EV_NONE = 0
EV_CURRENT = 1
EV_NUM = 2

SIZE_ELF32_HDR = 0x34
SIZE_ELF32_PHDR = 0x20
SIZE_ELF32_SHDR = 0x28
SIZE_ELF32_SYM = 0x10
SIZE_ELF32_DYN = 0x8

SIZE_ELF64_HDR = 0x40
SIZE_ELF64_PHDR = 0x38
SIZE_ELF64_SHDR = 0x40
SIZE_ELF64_SYM = 0x18
SIZE_ELF64_DYN = 0x10

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
SHT_LOOS = 0x60000000
SHT_HIOS = 0x6fffffff
SHT_LOPROC = 0x70000000
SHT_HIPROC = 0x7FFFFFFF
SHT_LOUSER = 0x80000000
SHT_HIUSER = 0xFFFFFFFF
def sh_type_tostr(t):
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
	if t in lookup:
		return lookup[t]
	if t >= SHT_LOOS and t <= SHT_HIOS:
		return 'OS'
	if t >= SHT_LOPROC and t <= SHT_HIPROC:
		return 'PROC'
	if t >= SHT_LOUSER and t <= SHT_HIUSER:
		return 'USER'
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
def phdr_type_tostr(x):
	lookup = {PT_NULL:"PT_NULL", PT_LOAD:"PT_LOAD", PT_DYNAMIC:"PT_DYNAMIC",
		PT_INTERP:"PT_INTERP", PT_NOTE:"PT_NOTE", PT_SHLIB:"PT_SHLIB",
		PT_PHDR:"PT_PHDR", PT_TLS:"PT_TLS"
	}
	if x in lookup:
		return lookup[x]
	if x >= PT_LOOS and x <= PT_HIOS:
		return 'OS'
	if x >= SHT_LOPROC and x <= SHT_HIPROC:
		strType = 'PROC'
	return 'UNKNOWN'

DT_NULL = 0
DT_NEEDED = 1
DT_PLTRELSZ = 2
DT_PLTGOT = 3
DT_HASH = 4
DT_STRTAB = 5
DT_SYMTAB = 6
DT_RELA = 7
DT_RELASZ = 8
DT_RELAENT = 9
DT_STRSZ = 10
DT_SYMENT = 11
DT_INIT = 12
DT_FINI = 13
DT_SONAME = 14
DT_RPATH = 15
DT_SYMBOLIC = 16
DT_REL = 17
DT_RELSZ = 18
DT_RELENT = 19
DT_PLTREL = 20
DT_DEBUG = 21
DT_TEXTREL = 22
DT_JMPREL = 23
DT_BIND_NOW = 24
DT_INIT_ARRAY = 25
DT_FINI_ARRAY = 26
DT_INIT_ARRAYSZ = 27
DT_FINI_ARRAYSZ = 28
DT_LOOS = 0x60000000
DT_HIOS = 0x6FFFFFFF
DT_LOPROC = 0x70000000
DT_HIPROC = 0x7FFFFFFF
def dynamic_type_tostr(x):
	lookup = { DT_NULL:"NULL", DT_NEEDED:"NEEDED", DT_PLTRELSZ:"PLTRELSZ",
		DT_PLTGOT:"PLTGOT", DT_HASH:"HASH", DT_STRTAB:"STRTAB", 
		DT_SYMTAB:"SYMTAB", DT_RELA:"RELA", DT_RELASZ:"RELASZ",
		DT_RELAENT:"RELAENT", DT_STRSZ:"STRSZ", DT_SYMENT:"SYMENT",
		DT_INIT:"INIT", DT_FINI:"FINI", DT_SONAME:"SONAME",
		DT_RPATH:"RPATH", DT_SYMBOLIC:"SYMBOLIC", DT_REL:"REL",
		DT_RELSZ:"RELSZ", DT_RELENT:"RELENT", DT_PLTREL:"PLTREL",
		DT_DEBUG:"DEBUG", DT_TEXTREL:"TEXTREL", DT_JMPREL:"JMPREL",
		DT_BIND_NOW:"BIND_NOW", DT_INIT_ARRAY:"INIT_ARRAY", DT_FINI_ARRAY:"FINI_ARRAY",
		DT_INIT_ARRAYSZ:"INIT_ARRAYSZ", DT_FINI_ARRAYSZ:"FINI_ARRAYSZ"
	}
	if x in lookup:
		return lookup[x]
	if x >= DT_LOOS and x <= DT_HIOS:
		return "OS"
	if x >= DT_LOPROC and x <= DT_HIPROC:
		return "PROC"
	return 'UNKNOWN'

# symbol bindings
STB_LOCAL = 0 
STB_GLOBAL = 1 
STB_WEAK = 2 
STB_LOPROC = 13
STB_HIPROC = 15
def symbol_binding_tostr(x):
	lookup = { STB_LOCAL:"LOCAL", STB_GLOBAL:"GLOBAL",
		STB_WEAK:"WEAK"
	}
	if x in lookup:
		return lookup[x]
	if x >= STB_LOPROC and x <= STB_HIPROC:
		return "PROC"
	return 'UNKNOWN'

# symbol types
STT_NOTYPE = 0 
STT_OBJECT = 1 
STT_FUNC = 2 
STT_SECTION = 3 
STT_FILE = 4 
STT_COMMON = 5 
STT_TLS = 6 
STT_LOPROC = 13
STT_HIPROC = 15
def symbol_type_tostr(x):
	lookup = { STT_NOTYPE:"NOTYPE", STT_OBJECT:"OBJECT",
		STT_FUNC:"FUNC", STT_SECTION:"SECTION", STT_FILE:"FILE",
		STT_COMMON:"COMMON", STT_TLS:"TLS"}
	if x in lookup:
		return lookup[x]
	if x >= STT_LOPROC and x <= STT_HIPROC:
		return "PROC"
	return 'UNKNOWN'

###############################################################################
# helper classes
###############################################################################
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

def isElf(fp):
	elfClass = None
	tmp = fp.tell()
	fp.seek(0)
	if fp.read(4) != "\x7fELF": 
		return False
	elfClass = unpack('B', fp.read(1))[0] # e_ident[EI_CLASS]
	if not (elfClass in [ELFCLASS32, ELFCLASS64]):
		return False
	eiData = unpack('B', fp.read(1))[0] # e_ident[EI_DATA]
	if not eiData in [ELFDATA2LSB, ELFDATA2MSB]:
		return False
	eiVersion = unpack('B', fp.read(1))[0]
	if not (eiVersion in [EV_CURRENT]): # e_ident[EI_VERSION]
		return False 
	fp.seek(tmp)
	return (True, elfClass)

def isElf64(fp):
	result = isElf(fp)
	return (result and result[1] == ELFCLASS64)

def isElf32(fp):
	result = isElf(fp)
	return (result and result[1] == ELFCLASS32)

###############################################################################
# main
###############################################################################

if __name__ == '__main__':
	sys.exit(-1)
