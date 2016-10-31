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
	fp = open(fpathIn, "rb")
	result = isElf64(fp)
	fp.close()

	return result

def tagReally(fpathIn, fpathOut):
	fp = open(fpathIn, "rb")

	# we want to be keep the convenience of writing tags to stdout with print()
	stdoutOld = None
	if fpathOut:
		stdoutOld = sys.stdout
		sys.stdout = open(fpathOut, "w")

	while 1:
		# test EOF
		tmp = fp.read(1)
		if tmp == "": 
			break
		fp.seek(-1, os.SEEK_CUR)

		hdrLen = 0
		offs = fp.tell()
		tmp = uint8(fp)
		fmt = None
		print "read byte 0x%02X" % tmp
		assert(tmp & 0x80)
		if tmp & 0x40:
			# new format
			fmt = 'new'
			tag = 0x3F & tmp
			octet1 = uint8(fp)
			print "octet1: 0x%X" % octet1
			if octet1 <= 191:
				hdrLen = 2
				bodyLen = octet1
			elif octet1 > 192 and octet1 <= 223:
				hdrLen = 3
				bodyLen = (octet1 - 192)*256 + uint8(fp) + 192
			elif octet1 > 223 and octet1 < 255:
				hdrLen = 2
				bodyLen = bodyLen = 1 << (octet1 & 0x1f)
			else:
				assert(octet1==255)
				hdrLen = 6
				bodyLen = uint32(fp)
		else:
			# old format
			fmt = 'old'
			length_type = tmp & 3
			tagId = (0x3C & tmp) >> 2
			if length_type == 0:
				# one-octet length
				hdrLen = 2
				bodyLen = uint8(fp)
			elif length_type == 1:
				hdrLen = 3
				bodyLen = uint16(fp)
			elif length_type == 2:
				hdrLen = 5
				bodyLen = uint32(fp)
			elif length_type == 3:
				hdrLen = 1
				fp.seek(0, os.SEEK_END)
				bodyLen = fp.tell()

		# header
		print "[0x%X,0x%X) 0x0 hdr %s \"%s\"" % \
			(offs, offs+hdrLen, fmt, tagToStr(tagId))
		# header 1/2 is tag byte
		print "[0x%X,0x%X) 0x0 tag=%d" % (offs, offs+1, tagId)
		# header 2/2 is length bytes (optional)
		if hdrLen > 1:
			print "[0x%X,0x%X) 0x0 length=0x%X" % (offs+1, offs+hdrLen-1, bodyLen)
		# body
		print "[0x%X,0x%X) 0x0 body" % (offs+hdrLen, offs+hdrLen+bodyLen)

		# seek to next packet
		fp.seek(offs + hdrLen + bodyLen)

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






