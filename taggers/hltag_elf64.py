#!/usr/bin/env python

import sys
import struct
import binascii

from hltag_elf import *
from hltag_lib import *

###############################################################################
# "main"
###############################################################################

if __name__ == '__main__':
	fp = open(sys.argv[1], "rb")
	if not isElf64(fp):
		sys.exit(-1)

	tag(fp, SIZE_ELF64_HDR, "elf64_hdr", 1)
	tmp = tag(fp, 4, "e_ident[0..4)")
	tmp = tagUint8(fp, "e_ident[EI_CLASS] (64-bit)")
	ei_data = uint8(fp, 1);
	tagUint8(fp, "e_ident[EI_DATA] %s" % ei_data_tostr(ei_data))
	assert(ei_data in [ELFDATA2LSB,ELFDATA2MSB])
	if ei_data == ELFDATA2LSB:
		setLittleEndian()
	elif ei_data == ELFDATA2MSB:
		setBigEndian()
	tmp = tagUint8(fp, "e_ident[EI_VERSION] (little-end)")
	tmp = tagUint8(fp, "e_ident[EI_OSABI]");
	tmp = tagUint8(fp, "e_ident[EI_ABIVERSION]");
	tmp = tag(fp, 7, "e_ident[EI_PAD]");
	tagUint16(fp, "e_type")
	tagUint16(fp, "e_machine")
	tagUint32(fp, "e_version")
	tagUint64(fp, "e_entry")
	e_phoff = tagUint64(fp, "e_phoff")
	e_shoff = tagUint64(fp, "e_shoff")
	tagUint32(fp, "e_flags")
	tagUint16(fp, "e_ehsize")
	tagUint16(fp, "e_phentsize")
	e_phnum = tagUint16(fp, "e_phnum")
	tagUint16(fp, "e_shentsize")
	e_shnum = tagUint16(fp, "e_shnum")
	e_shstrndx = tagUint16(fp, "e_shstrndx")
	
	# read the string table
	fp.seek(e_shoff + e_shstrndx*SIZE_ELF64_SHDR)
	tmp = fp.tell()
	fmt = {ELFDATA2LSB:'<IIQQQQ', ELFDATA2MSB:'>IIQQQQ'}[ei_data]
	(a,b,c,d,sh_offset,sh_size) = struct.unpack(fmt, fp.read(40));
	fp.seek(sh_offset)
	scnStrTab = StringTable(fp, sh_size)
	
	# read all section headers
	dynamic = None
	symtab = None
	strtab = None
	fp.seek(e_shoff)
	for i in range(e_shnum):
		oHdr = fp.tell()
		sh_name = tagUint32(fp, "sh_name")
		sh_type = uint32(fp, 1)
		tag(fp, 4, "sh_type=0x%X (%s)" % \
			(sh_type, sh_type_tostr(sh_type)))
		sh_flags = uint64(fp, 1)
		tag(fp, 8, "sh_flags=0x%X (%s)" % \
			(sh_flags, sh_flags_tostr(sh_flags)))
		tagUint64(fp, "sh_addr")
		sh_offset = tagUint64(fp, "sh_offset")
		sh_size = tagUint64(fp, "sh_size")
		tagUint32(fp, "sh_link")
		tagUint32(fp, "sh_info")
		tagUint64(fp, "sh_addralign")
		tagUint64(fp, "sh_entsize")
	
		strType = sh_type_tostr(sh_type)
		strName = scnStrTab[sh_name]
	
		# store info on special sections
		if strName == '.dynamic':
			dynamic = [sh_offset, sh_size]
		if strName == '.symtab':
			symtab = [sh_offset, sh_size]
		if strName == '.strtab':
			strtab = [sh_offset, sh_size]

		print '[0x%X,0x%X) 0x0 elf64_shdr "%s" %s' % \
			(oHdr, fp.tell(), scnStrTab[sh_name], strType)
	
		if(not sh_type in [SHT_NULL, SHT_NOBITS]):
			print '[0x%X,0x%X) 0x0 section "%s" contents' % \
				(sh_offset, sh_offset+sh_size, scnStrTab[sh_name])

	# certain sections we analyze deeper...
	if strtab:
		[offs,size] = strtab
		fp.seek(offs)
		strTab = StringTable(fp, size)

	if dynamic:
		# .dynamic is just an array of Elf64_Dyn entries
		[offs,size] = dynamic
		fp.seek(offs)
		while fp.tell() < (offs + size):
			tmp = fp.tell()
			d_tag = uint64(fp, 1)
			tagStr = dynamic_type_tostr(d_tag)
			tag(fp, 8, "d_tag:0x%X (%s)" % (d_tag, tagStr))
			tagUint64(fp, "val_ptr")
			fp.seek(tmp)
			tag(fp, SIZE_ELF64_DYN, "Elf64_Dyn (%s)" % tagStr)		

			if d_tag == DT_NULL:
				break

	if symtab:
		# .symbtab is an array of Elf64_Sym entries
		# note that Elf64_Sym differs from Elf32_Sym beyond field sizes
		[offs,size] = symtab
		fp.seek(offs)
		while fp.tell() < (offs + size):
			tmp = fp.tell()
			st_name = uint32(fp, 1)
			nameStr = strTab[st_name]
			tag(fp, 4, "st_name=0x%X \"%s\"" % (st_name,nameStr))
			st_info = uint8(fp, 1)
			bindingStr = symbol_binding_tostr(st_info >> 4)
			typeStr = symbol_type_tostr(st_info & 0xF)
			tag(fp, 1, "st_info bind:%d(%s) type:%d(%s)" % \
				(st_info>>4, bindingStr, st_info&0xF, typeStr))
			st_other = tagUint8(fp, "st_other")
			st_shndx = tagUint16(fp, "st_shndx")
			st_value = tagUint64(fp, "st_value")
			st_size = tagUint64(fp, "st_size")
			fp.seek(tmp)
			tag(fp, SIZE_ELF64_SYM, "Elf64_Sym \"%s\"" % nameStr)

	# read program headers
	fp.seek(e_phoff)
	for i in range(e_phnum):
		oHdr = fp.tell()
		p_type = tagUint32(fp, "p_type")
		tagUint32(fp, "p_flags")
		tagUint64(fp, "p_offset")
		tagUint64(fp, "p_vaddr")
		tagUint64(fp, "p_paddr")
		tagUint64(fp, "p_filesz")
		tagUint64(fp, "p_memsz")
		tagUint64(fp, "p_align")
	
		strType = phdr_type_tostr(p_type)
	
		print '[0x%X,0x%X) 0x0 elf64_phdr %d %s' % \
			(oHdr, fp.tell(), i, strType)
	
	fp.close()
	sys.exit(0)

