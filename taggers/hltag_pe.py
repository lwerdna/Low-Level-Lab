#!/usr/bin/env python

import os
import sys

from hltag_lib import *
from struct import pack, unpack

IMAGE_NUMBEROF_DIRECTORY_ENTRIES = 16
IMAGE_SIZEOF_SHORT_NAME = 8
IMAGE_SIEOF_SECTION_HEADER = 40
IMAGE_SIZEOF_FILE_HEADER = 20
IMAGE_SIZEOF_SECTION_HEADER = 40
IMAGE_SIZEOF_SYMBOL = 18
IMAGE_SIZEOF_ARCHIVE_MEMBER_HDR = 60

IMAGE_FILE_MACHINE_I386 = 0x014c
IMAGE_FILE_MACHINE_IA64 = 0x0200
IMAGE_FILE_MACHINE_AMD64 = 0x8664

# pe64 vs. pe32:
# 1) different id in image_nt_headers.image_file_header.Machine
# 2) image_optional_header at 0xE0 (224 bytes) is replaced by 
#   image_optional_header64 at 0xF0 (240 bytes) with:
#   2.1) BaseOfData is GONE, its bytes get absorbed into ImageBase, growing it
#		from 4 bytes to 8 bytes
#   2.2) all these fields grow from 4 to 8 bytes:
#   - SizeOfStackReserve, SizeOfStackCommit, SizeOfHeapReserve, SizeOfHeapCommit
#	 all increase from 4 bytes to 8 bytes
#
# this should result in an Image_Optional_header that is 0xF0 bytes

def idFile(fp):
	fpos = fp.tell()

	# get file size
	fp.seek(0, os.SEEK_END)
	fileSize = fp.tell()
	#print "fileSize: 0x%X (%d)" % (fileSize, fileSize)

	# file large enough to hold IMAGE_DOS_HEADER ?
	if fileSize < 0x40: 
		#print "file too small to hold IMAGE_DOS_HEADER"
		fp.seek(fpos)
		return False

	# is IMAGE_DOS_HEADER.e_magic == "MZ" ?
	fp.seek(0)
	if fp.read(2) != "MZ": 
		#print "missing MZ identifier"
		fp.seek(fpos)
		return False

	# get IMAGE_DOS_HEADER.e_lfanew
	fp.seek(0x3C)
	e_lfanew = unpack('<I', fp.read(4))[0]

	# is file large enough to hold IMAGE_NT_HEADERS ?
	if fileSize < (e_lfanew + 0x108): 
		#print "file too small to hold IMAGE_NT_HEADERS"
		fp.seek(fpos)
		return False

	# does IMAGE_NT_HEADERS.signature == "PE" ?
	fp.seek(e_lfanew)
	if fp.read(4) != "PE\x00\x00": 
		#print "missing PE identifier"
		fp.seek(fpos)
		return False

	#
	result = "unknown"
	(machine,) = unpack('<H', fp.read(2))
	if machine==IMAGE_FILE_MACHINE_AMD64:
		result = "pe64"
	if machine==IMAGE_FILE_MACHINE_I386:
		result = "pe32"
	fp.seek(fpos)
	return result

def dataDirIdxToStr(idx):
    lookup = [ "EXPORT", "IMPORT", "RESOURCE", "EXCEPTION", 
        "SECURITY", "BASERELOC", "DEBUG", "ARCHITECTURE", 
        "GLOBALPTR", "TLS", "LOAD_CONFIG", "BOUND_IMPORT", 
        "IAT", "DELAY_IMPORT", "COM_DESCRIPTOR" ]

    if idx>=0 and idx<len(lookup):
        return lookup[idx]

    return "UNKNOWN"

def relocTypeToStr(t, machine=''):
	lookup = ["ABSOLUTE", "HIGH", "LOW", "HIGHLOW", "HIGHADJ"] # index [0,4]
	lookup.append({'':'UNKNOWN', 'mips':'MIPS_JMPADDR', # index 5
		'arm':'ARM_MOV32', 'thumb':'ARM_MOV32', 
		'risc-v':'RISCV_HIGH20'}[machine])
	lookup.append('RESERVED') # index 6
	lookup.append({'':'UNKNOWN', 'arm':'ARM_MOV32', # index 7
		'thumb':'ARM_MOV32', 'risc-v':'RISCV_HIGH20'}[machine])
	lookup += ['RISCV_LOW12S'] # index 8
	lookup += ['JMPADDR16'] # index 9
	lookup += ['DIR64'] # index 10
	return lookup[t]

def tagReloc(fp, size, machine=''):
	end = fp.tell() + size
	while fp.tell() < end:
		#print "end is: 0x%X and tell is: 0x%X" % (end, fp.tell())
		oBlockStart = fp.tell()

		# peek on VirtualAddress and SizeOfBlock
		if uint64(fp, True) == 0:
			print '[0x%X,0x%X) 0x0 reloc block NULL' % (oBlockStart, oBlockStart+8)
			break;

		VirtualAddress = tagUint32(fp, "VirtualAddress")	
		SizeOfBlock = tagUint32(fp, "SizeOfBlock")
		nEntries = (SizeOfBlock-8)/2
		print '[0x%X,0x%X) 0x0 reloc block 0x%X (%d entries)' % \
		  (oBlockStart, oBlockStart+SizeOfBlock, VirtualAddress, nEntries)

		for i in range(nEntries):
			toto = uint16(fp, True)
			rtype = (toto&0xF000)>>12
			rtypeStr = relocTypeToStr(rtype)
			roffs = toto&0xFFF
			tag(fp, 2, "reloc entry %d=%s offset=0x%X" % (rtype,rtypeStr,roffs))
			
###############################################################################
# main
###############################################################################

if __name__ == '__main__':
	sys.exit(-1)	
	
