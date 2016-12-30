#!/usr/bin/env python

import os
import sys
import struct
import binascii

scriptDir = os.path.dirname(os.path.abspath(__file__))
sys.path.append(scriptDir)
from hltag_lib import *

ENDIAN_CONSTANT = 0x12345678
REVERSE_ENDIAN_CONSTANT = 0x78563412

DEX_FILE_MAGIC_09 = "dex\x0a009\x00"
DEX_FILE_MAGIC_13 = "dex\x0a013\x00"
DEX_FILE_MAGIC_35 = "dex\x0a035\x00"
DEX_FILE_MAGIC_37 = "dex\x0a037\x00"
def magicToStr(magic):
	if magic == DEX_FILE_MAGIC_09:
		return 'magic v09 (M3 release Nov-Dec 2007)'
	if magic == DEX_FILE_MAGIC_13:
		return 'magic v13 (M5 release Feb-Mar 2008)'
	if magic == DEX_FILE_MAGIC_35:
		return 'magic v35'
	if magic == DEX_FILE_MAGIC_09:
		return 'magic v37 (Android 7.0)'
	return 'uknown'

###############################################################################
# "main"
###############################################################################

if __name__ == '__main__':
	fp = open(sys.argv[1], "rb")

	magic = fp.read(8)
	if not (magic in [DEX_FILE_MAGIC_35, DEX_FILE_MAGIC_37]):
		sys.exit(-1);

	print "[0x0,0x8) 0x0 %s" % magicToStr(magic)
	tagUint32(fp, 'checksum (adler32)')
	tag(fp, 20, 'signature (sha1)')
	tagUint32(fp, 'file_size')
	hdrSize = tagUint32(fp, 'header_size')
	assert(hdrSize == 0x70)
	endian_tag = tagUint32(fp, 'endian_tag')
	assert(endian_tag == ENDIAN_CONSTANT)
	tagUint32(fp, 'link_size')
	tagUint32(fp, 'link_off')
	tagUint32(fp, 'map_off')
	string_ids_size = tagUint32(fp, 'string_ids_size')
	string_ids_off = tagUint32(fp, 'string_ids_off')
	tagUint32(fp, 'type_ids_size')
	tagUint32(fp, 'type_ids_off')
	tagUint32(fp, 'proto_ids_size')
	tagUint32(fp, 'proto_ids_off')
	tagUint32(fp, 'field_ids_size')
	tagUint32(fp, 'field_ids_offs')
	tagUint32(fp, 'methods_ids_size')
	tagUint32(fp, 'methods_ids_off')
	tagUint32(fp, 'class_defs_size')
	tagUint32(fp, 'class_defs_off')
	tagUint32(fp, 'data_size')
	tagUint32(fp, 'data_off')
	print "[0x0,0x70) 0x0 header"

	# visit string_id_item array
	fp.seek(string_ids_off)
	print "[0x%X,0x%X) 0x0 string_ids (%d entries)" % \
		(string_ids_off, string_ids_off+string_ids_size*4, string_ids_size)
	offs = []
	for i in range(string_ids_size):
		off = tagUint32(fp, "string_id_item %d/%d" % (i+1, string_ids_size))
		offs.append(off)
	# visit string_data_item array
	for o in offs:
		fp.seek(o)
		utf16_size = tagUleb128(fp, "utf16_size")
		data = tagDataUntil(fp, "\x00", "data")
		print "[0x%X,0x%X) 0x0 string_data_item \"%s\"" % (o, fp.tell(), data[0:-1])

	fp.close()
	sys.exit(0)

