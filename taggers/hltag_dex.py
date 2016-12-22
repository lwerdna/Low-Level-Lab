#!/usr/bin/python

import os
import sys
import struct
import binascii
from hlab_taglib import *

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
# API that taggers must implement
###############################################################################

def tagTest(fpathIn):
	fp = open(fpathIn, 'rb')
	tmp = fp.read(8)
	fp.close()
	return tmp in [DEX_FILE_MAGIC_35, DEX_FILE_MAGIC_37]

def tagReally(fpathIn, fpathOut):
	fp = open(fpathIn, "rb")

	# we want to be keep the convenience of writing tags to stdout with print()
	stdoutOld = None
	if fpathOut:
		stdoutOld = sys.stdout
		sys.stdout = open(fpathOut, "w")

	magic = fp.read(8)
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
	tagUint32(fp, 'string_ids_size')
	tagUint32(fp, 'string_ids_off')
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
	print "[0x0,0x70) 0x0 header_item"

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

