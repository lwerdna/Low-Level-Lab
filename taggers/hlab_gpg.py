#!/usr/bin/python

import os
import sys
import struct
import binascii
from hlab_taglib import *

TAG_RESERVED = 0
TAG_PUB_KEY_ENCR_SESS_KEY = 1
TAG_SIGNATURE = 2
TAG_SYMKEY_ENCR_SESS_KEY = 3
TAG_ONE_PASS_SIG = 4
TAG_SECRET_KEY = 5
TAG_PUB_KEY = 6
TAG_SECRET_SUBKEY = 7
TAG_COMPR_DATA = 8
TAG_SYMM_ENCR_DATA = 9
TAG_MARKER = 10
TAG_LITERAL_DATA = 11
TAG_TRUST = 12
TAG_USER_ID = 13
TAG_PUB_SUBKEY = 14
TAG_USER_ATTR = 17
TAG_SYMM_ENCR_INTEGRITY_PROT_DATA = 18
TAG_MODIF_DETECT_CODE = 19
TAG_PRIVATE_EXPERIMENTAL_0 = 0
TAG_PRIVATE_EXPERIMENTAL_1 = 1
TAG_PRIVATE_EXPERIMENTAL_2 = 2
TAG_PRIVATE_EXPERIMENTAL_3 = 3

def tagToStr(tag):
	lookup = {TAG_RESERVED:"reserved", TAG_PUB_KEY_ENCR_SESS_KEY:'public-key encr session key',
		TAG_SIGNATURE:'signature', TAG_SYMKEY_ENCR_SESS_KEY:'symmetric-key encr session key',
		TAG_ONE_PASS_SIG:'one-pass signature', TAG_SECRET_KEY:'secret-key', TAG_PUB_KEY:'public-key',
		TAG_SECRET_SUBKEY:'secret-subkey', TAG_COMPR_DATA:'compressed data', 
		TAG_SYMM_ENCR_DATA:'symmetric encr data', TAG_MARKER:'marker', 
		TAG_LITERAL_DATA:'literal data', TAG_TRUST:'trust', TAG_USER_ID:'user id',
		TAG_PUB_SUBKEY:'public-subkey', TAG_USER_ATTR:'user attribute', 
		TAG_SYMM_ENCR_INTEGRITY_PROT_DATA:'symmetric encrypted and integrity protected data',
		TAG_MODIF_DETECT_CODE:'modification detection code'
	}

	if tag in lookup:
		return lookup[tag]

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

		tagId = 0
		body = ''

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
					
				body += fp.read(bodyLen)

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

			body = fp.read(bodyLen)

		# mark the whole packet
		print "[0x%X,0x%X) 0x0 %s packet" % \
			(oPacket, fp.tell(), tagToStr(tagId))

		# certain packets we go deeper
		if tagId == TAG_SYMKEY_ENCR_SESS_KEY:

		else:

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






