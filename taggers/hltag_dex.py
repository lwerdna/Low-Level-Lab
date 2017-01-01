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

def typeDescriptorToString(td):
	lookup = {'V':'void', 'Z':'bool', 'B':'byte', 'S':'short', 'C':'char',
		'I':'int', 'J':'long', 'F':'float', 'D':'double'}

	if td in lookup:
		return lookup[td]

	return ''

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
	type_ids_size = tagUint32(fp, 'type_ids_size')
	type_ids_off = tagUint32(fp, 'type_ids_off')
	proto_ids_size = tagUint32(fp, 'proto_ids_size')
	proto_ids_off = tagUint32(fp, 'proto_ids_off')
	field_ids_size = tagUint32(fp, 'field_ids_size')
	field_ids_off = tagUint32(fp, 'field_ids_off')
	methods_ids_size = tagUint32(fp, 'methods_ids_size')
	methods_ids_off = tagUint32(fp, 'methods_ids_off')
	class_defs_size = tagUint32(fp, 'class_defs_size')
	class_defs_off = tagUint32(fp, 'class_defs_off')
	data_size = tagUint32(fp, 'data_size')
	data_off = tagUint32(fp, 'data_off')
	print "[0x0,0x70) 0x0 header"

	# mark data section
	print "[0x%X,0x%X) 0x00 data section" % (data_off, data_off+data_size)

	# visit string_id_item array
	stringIdxToString = []
	fp.seek(string_ids_off)
	if string_ids_size > 1:
		print "[0x%X,0x%X) 0x0 string_ids (%d entries)" % \
			(string_ids_off, string_ids_off+string_ids_size*4, string_ids_size)
	stringIdOffsets = []
	for i in range(string_ids_size):
		off = uint32(fp, True)
		stringIdOffsets.append(off)

		# seek to offset and grab string
		anchor = fp.tell()
		fp.seek(off)
		uleb128(fp)
		string = dataUntil(fp, "\x00")
		if string[-1]=='\x00': string=string[0:-1]
		#print "at offset 0x%X, found string %s" % (off, repr(string))
		stringIdxToString.append(string)
		fp.seek(anchor)

		# tag 
		tagUint32(fp, "string_id_item %d/%d" % (i+1, string_ids_size))

	# visit type_id_item array
	typeIdxToString = []
	fp.seek(type_ids_off)
	if type_ids_size > 1:
		print "[0x%X,0x%X) 0x0 type_ids (%d entries)" % \
			(type_ids_off, type_ids_off+type_ids_size*4, type_ids_size)
	for i in range(type_ids_size):
		off = uint32(fp)
		typeIdxToString.append(stringIdxToString[off])

		print "[0x%X,0x%X) 0x0 type_id_item %d/%d %s" % \
			(fp.tell()-4, fp.tell(), i+1, type_ids_size, repr(stringIdxToString[off]))

	#print '--------'
	#print typeIdxToString
	#print '--------'

	# visit proto_id_item array
	fp.seek(proto_ids_off)
	if proto_ids_size > 1:
		print "[0x%X,0x%X) 0x0 proto_ids (%d entries)" % \
			(proto_ids_off, proto_ids_off+proto_ids_size*12, proto_ids_size)
	for i in range(proto_ids_size):
		shorty_idx = tagUint32(fp, "shorty_idx")
		return_type_idx = uint32(fp, True)
		rtiStr = typeIdxToString[return_type_idx]
		tagUint32(fp, "return_type_idx %s %s" % (repr(typeIdxToString[return_type_idx]), rtiStr))
		tagUint32(fp, "parameters_off")
		print "[0x%X,0x%X) 0x0 proto_id_item %d/%d %s" % \
			(fp.tell()-12, fp.tell(), i+1, proto_ids_size, repr(stringIdxToString[shorty_idx]))

	# visit field_id_item array
	if field_ids_off:
		fp.seek(field_ids_off)
		if field_ids_size > 1:
			print "[0x%X,0x%X) 0x0 field_ids (%d entries)" % \
				(field_ids_off, field_ids_off+field_ids_size*8, field_ids_size)
		for i in range(field_ids_size):
			tagUint16(fp, "class_idx")
			tagUint16(fp, "type_idx")
			name_idx = tagUint32(fp, "name_idx")
			print "[0x%X,0x%X) 0x0 field_id_item %d/%d %s" % \
				(fp.tell()-8, fp.tell(), i+1, field_ids_size, repr(stringIdxToString[name_idx]))

	# visit methods_id_item array
	if methods_ids_off:
		fp.seek(methods_ids_off)
		if methods_ids_size > 1:
			print "[0x%X,0x%X) 0x0 methods_ids (%d entries)" % \
				(methods_ids_off, methods_ids_off+methods_ids_size*8, methods_ids_size)
		for i in range(methods_ids_size):
			tagUint16(fp, "class_idx")
			tagUint16(fp, "proto_idx")
			name_idx = tagUint32(fp, "name_idx")
			print "[0x%X,0x%X) 0x0 methods_id_item %d/%d %s" % \
				(fp.tell()-8, fp.tell(), i+1, methods_ids_size, repr(stringIdxToString[name_idx]))

	# visit class_def_item array
	if class_defs_off:
		fp.seek(class_defs_off)
		if class_defs_size > 1:
			print "[0x%X,0x%X) 0x0 class_defs (%d entries)" % \
				(class_defs_off, class_defs_off+class_defs_size*32, class_defs_size)
		for i in range(class_defs_size):
			class_idx = tagUint32(fp, "class_idx")
			tagUint32(fp, "access_flags")
			tagUint32(fp, "superclass_idx")
			tagUint32(fp, "interfaces_off")
			tagUint32(fp, "source_file_idx")
			tagUint32(fp, "annotations_off")
			tagUint32(fp, "class_data_off")
			tagUint32(fp, "static_values_off")
			if class_idx < len(typeIdxToString):
				print "[0x%X,0x%X) 0x0 class_def_item %d/%d %s" % \
					(fp.tell()-32, fp.tell(), i+1, class_defs_size, repr(typeIdxToString[class_idx]))
			else:
				print "[0x%X,0x%X) 0x0 class_def_item %d/%d 'err'" % \
					(fp.tell()-32, fp.tell(), i+1, class_defs_size)

	# visit string_data_item array
	for o in stringIdOffsets:
		fp.seek(o)
		utf16_size = tagUleb128(fp, "utf16_size")
		data = tagDataUntil(fp, "\x00", "data")
		print "[0x%X,0x%X) 0x0 string_data_item \"%s\"" % (o, fp.tell(), data[0:-1])

	fp.close()
	sys.exit(0)

