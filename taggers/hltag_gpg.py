#!/usr/bin/env python

import os
import sys
import struct
import binascii
from hltag_lib import *

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

# reference section 9.2 in rfc4880
def symAlgoToStr(algo):
	lookup = {0:'plaintext/unencrypted', 1:'IDEA', 2:'TripleDES', 3:'CAST5',
		4:'Blowfish', 5:'Reserved', 6:'Reserved', 7:'AES128', 8:'AES192',
		9:'AES256', 10:'Twofish'}

	if algo in lookup:
		return lookup[algo]

	if algo >= 100 and algo <= 110:
		return "private/experimental"

	return 'unknown'

# reference section 3.7.1 in rfc4880
def s2kToStr(algo):
	if algo == 0:
		return 'Simple S2K'
	if algo == 1:
		return 'Salted S2K'
	if algo == 2:
		return 'Reserved'
	if algo == 3:
		return 'Iterated+Salted S2K'
	if algo >= 100 and algo <= 110:
		return 'private/experimental'

	return 'unknown'

# reference section 9.4 in rfc4880
def hashAlgoToStr(algo):
	lookup = ['invalid', 'md5', 'sha1', 'ripe-md', 'reserved', 'reserved',
		'reserved', 'reserved', 'sha256', 'sha384', 'sha512', 'sha224']

	if algo >= 0 and algo < len(lookup):
		return lookup[algo]

	if algo >= 100 and algo <= 110:
		return 'private/experimental'

	return 'unknown'

# reference section 9.3 in rfc4880
def comprAlgoToStr(algo):
	if algo == 0:
		return 'uncompressed'
	if algo == 1:
		return 'zip [rfc1951]'
	if algo == 2:
		return 'zlib [rfc1950]'
	if algo == 3:
		return 'bzip2 [bz2]'
	if algo >= 100 and algo <= 110:
		return 'private/experimental'

	return 'unknown'

def litDataFmtToStr(fmt):
	if fmt == ord('b'):
		return 'binary'
	if fmt == ord('t'):
		return 'text'
	if fmt == ord('u'):
		return 'utf-8'
	return 'unknown'

###############################################################################
# "main"
###############################################################################

if __name__ == '__main__':
	# for now, test just the file extension
	# TODO: see if file would make sense if we treated its bytes like packets
	if not re.match(r'^.*\.gpg$', sys.argv[1]):
		sys.exit(-1)

	fp = open(sys.argv[1], "rb")
	
	# for each packet
	while not IsEof(fp):
		(hdrLen,bodyLen) = (0,0)
		oPacket = fp.tell()

		packetTag = tagUint8(fp, "packet tag byte")
		assert(packetTag & 0x80)
	
		hdrStyle = 'unknown'
		tagId = 0
		body = ''
			
		# parse new format
		if packetTag & 0x40:
			hdrStyle = 'new'
			tagId = 0x3F & packetTag
			
			while 1:
				oLen = fp.tell()
				octet1 = uint8(fp)
				octet2 = uint8(fp)
				fp.seek(-2, os.SEEK_CUR) 

				partial = False
				bodyLen = 0
				if octet1 <= 191:
					hdrLen = 2
					bodyLen = octet1
					tag(fp, 1, "length (direct): 0x%X" % bodyLen)
				elif octet1 >= 192 and octet1 <= 223:
					hdrLen = 3
					bodyLen = (octet1 - 192)*256 + octet2 + 192
					tag(fp, 2, "length (calculated): 0x%X" % bodyLen)
				elif octet1 >= 224 and octet1 <= 254:
					hdrLen = 2
					bodyLen = 1 << (octet1 & 0x1f)
					tag(fp, 1, "length (partial): 0x%X" % bodyLen)
					partial = True
				else:
					hdrLen = 5
					bodyLen = tagUint32(fp, "len")
					tag(fp, 1, "length (direct): 0x%X" % bodyLen)
					
				body += fp.read(bodyLen)

				if IsEof(fp) or not partial: 
					break

		# parse old format
		else:
			hdrStyle = 'old'
			length_type = packetTag & 3
			tagId = (0x3C & packetTag) >> 2

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
				# length extends to end of file
				hdrLen = 1
				fp.seek(0, os.SEEK_END)
				bodyLen = fp.tell() - (oPacket + 1)

			body = fp.read(bodyLen)

		# mark the whole packet
		print "[0x%X,0x%X) 0x0 header (%s)" % (oPacket, oPacket+hdrLen, hdrStyle)
		print "[0x%X,0x%X) 0x0 body" % (oPacket+hdrLen, oPacket+hdrLen+bodyLen)
		print "[0x%X,0x%X) 0x0 %s packet (Tag %d)" % \
			(oPacket, fp.tell(), tagToStr(tagId), tagId)

		oPacketEnd = fp.tell()

		# certain packets we go deeper
		fp.seek(oPacket + hdrLen)

		if tagId == TAG_SYMKEY_ENCR_SESS_KEY:
			tagUint8(fp, "version");
			algoId = uint8(fp, True)
			tagUint8(fp, "algorithm (%s)" % symAlgoToStr(algoId))
			s2kId = uint8(fp, True)
			tagUint8(fp, "S2K (%s)" % s2kToStr(s2kId))
		
			if s2kId == 3: # iterated and salted
				hashId = uint8(fp, True)
				tagUint8(fp, "hash (%s)" % hashAlgoToStr(hashId))
				tag(fp, 8, "salt");
				countCoded = uint8(fp, True)
				count = (16 + (countCoded & 0xF)) << ((countCoded >> 4) + 6)
				tagUint8(fp, "count (decoded: %d)" % count)
		elif tagId == TAG_COMPR_DATA:
			algoId = uint8(fp, True)
			tagUint8(fp, "algorithm (%s)" % comprAlgoToStr(algoId))
			tag(fp, oPacketEnd - fp.tell(), "compressed data")

		elif tagId == TAG_LITERAL_DATA:
			fmt = uint8(fp, True)
			tagUint8(fp, "format (%s)" % litDataFmtToStr(fmt))
			lenFile = tagUint8(fp, "filename length")
			assert(lenFile < 256)
			tag(fp, lenFile, "filename")
			tagUint32(fp, "date")
			tag(fp, oPacketEnd - fp.tell(), "data")
	
		# next packet
		fp.seek(oPacketEnd)
		
	fp.close()
	sys.exit(0)
