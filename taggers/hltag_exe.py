#!/usr/bin/env python

import os
import sys
import struct
import binascii

scriptDir = os.path.dirname(os.path.abspath(__file__))
sys.path.append(scriptDir)
from hltag_lib import *

###############################################################################
# "main"
###############################################################################

# three main parts
# header
# relocation table
# binary code

if __name__ == '__main__':
	fp = open(sys.argv[1], "rb")

	# mz header
	if not fp.read(2) == 'MZ':
		sys.exit(-1)

	# cannot by 40h (new types of exe, like NE,LE,etc.)
	fp.seek(0x18)
	if fp.read(2) == 0x40:
		sys.exit(-1)

	fp.seek(0)
	tag(fp, 2, "MZ signature")
	tagUint16(fp, 'bytes_in_last_block')		# bytes in last 512-byte page
	tagUint16(fp, 'blocks_in_file')				# total number of 512-byte pages
	num_relocs = tagUint16(fp, 'num_relocs')	# number of relocation entries
	header_paragraphs = tagUint16(fp, 'header_paragraphs')			# header size in paragraphs
	tagUint16(fp, 'min_extra_paragraphs')
	tagUint16(fp, 'max_extra_paragraphs')
	tagUint16(fp, 'ss')
	tagUint16(fp, 'sp')
	tagUint16(fp, 'checksum (or 0)')
	tagUint16(fp, 'ip')
	tagUint16(fp, 'cs')
	reloc_table_offset = tagUint16(fp, 'reloc_table_offset')
	tagUint16(fp, 'overlay_number')
	
	# header is expanded by a lot of programs to store their copyright information
	fp.seek(0)		
	tag(fp, 16*header_paragraphs, 'EXE header', True)

	#if reloc_table_offset and reloc_table_offset > 0x1C:
	#	gap = reloc_table_offset - 0x1C
	#	tag(fp, gap, "expanded header", True)

	# do relocations
	fp.seek(reloc_table_offset)

	if num_relocs > 0:
		tag(fp, 4*num_relocs, "relocation table", True)

	for k in range(num_relocs):
		tag(fp, 4, "EXE_RELOC", True)
		tagUint16(fp, 'offset')
		tagUint16(fp, 'segment')

	fp.close()
	sys.exit(0)

