from taglib import *
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
			
				
		
	
