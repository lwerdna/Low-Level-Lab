#!/usr/bin/python

import os
import sys
import struct
import binascii
from hlab_taglib import *

def tagToStr(tag):
	lookup = {0:"reserved", 1:'public-key encr session key',
		2:'signature', 3:'symmetric-key encr session key',
		4:'one-pass signature', 5:'secret-key', 6:'public-key',
		7:'secret-subkey', 8:'compressed data', 9:'symmetric encr data',
		10:'marker', 11:'literal data', 12:'trust', 13:'user id',
		14:'public-subkey', 17:'user attribute', 
		18:'symmetric encrypted and integrity protected data',
		19:'modification detection code'
	}

	if tag in lookup:
		return lookup[tag]

	if tag in [60,61,62,63]:
		return 'private/experimental'

	return 'unknown'

###############################################################################
# API that taggers must implement
###############################################################################

def tagTest(fpathIn):
	return True

def tagReally(fpathIn, fpathOut):
	fp = open(fpathIn, "rb")

	# we want to be keep the convenience of writing tags to stdout with print()
	stdoutOld = None
	if fpathOut:
		stdoutOld = sys.stdout
		sys.stdout = open(fpathOut, "w")

	# for each packet
	while not IsEof(fp):
		hdrLen = 0
		oPacket = fp.tell()

		tagByte = tagUint8(fp, "tag byte")
		assert(tagByte & 0x80)

		tagId = None

		if tagByte & 0x40:
			# new format
			tagId = 0x3F & tagByte
			
			while 1:
				oLen = fp.tell()
				octet1 = uint8(fp)
				octet2 = uint8(fp)
				fp.seek(-2, os.SEEK_CUR) 

				partial = False
				bodyLen = 0
				if octet1 <= 191:
					bodyLen = octet1
					tag(fp, 1, "length (direct): 0x%X" % bodyLen)
				elif octet1 >= 192 and octet1 <= 223:
					bodyLen = (octet1 - 192)*256 + octet2 + 192
					tag(fp, 2, "length (calculated): 0x%X" % bodyLen)
				elif octet1 >= 224 and octet1 <= 254:
					bodyLen = 1 << (octet1 & 0x1f)
					tag(fp, 1, "length (partial): 0x%X" % bodyLen)
					partial = True
				else:
					bodyLen = tagUint32(fp, "len")
					tag(fp, 1, "length (direct): 0x%X" % bodyLen)
					
				tag(fp, bodyLen, "body")

				if IsEof(fp) or not partial: 
					break
		else:
			# old format

			length_type = tagByte & 3
			tagId = (0x3C & tagByte) >> 2
			bodyLen = 0

			if length_type == 0:
				# one-octet length
				hdrLen = 2
				bodyLen = tagUint8(fp, "len")
			elif length_type == 1:
				hdrLen = 3
				bodyLen = tagUint16(fp, "len")
			elif length_type == 2:
				hdrLen = 5
				bodyLen = tagUint32(fp, "len")
			elif length_type == 3:
				hdrLen = 1
				fp.seek(0, os.SEEK_END)
				bodyLen = fp.tell() - oPacket + 1

			tag(fp, bodyLen, "body")

		# header
		print "[0x%X,0x%X) 0x0 %s packet" % \
			(oPacket, fp.tell(), tagToStr(tagId))

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






