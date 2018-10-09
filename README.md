Low Level Laboratory


# Compiler LAB (clab)
* a little GUI to watch your compilers generate code

# Assembler LAB (alab)
* a little GUI that assembles for various architectures by calling into llvm libs

# Hex LAB (hlab)
Hlab is a hex editor with an extremely rapid way to learn about new binary formats and present that format in a TreeView. 

External programs call "taggers" are invoked by hlab for decomposing input binaries. They take, as input, the path to the input binary. They print, on stdout, regions which should carry a label, called a tag.
Taggers can be written in any language; the included ones are made with python.

Below is some example output from the included tagger hltag_MachO.py.
```
[0x0,0x20) 0x0 mach_header_64
[0x0,0x4) 0x0 magic=FEEDFACF (MH_MAGIC_64)
[0x4,0x8) 0x0 cputype=01000007 (CPU_TYPE_X86_64)
[0x8,0xC) 0x0 cpusubtype=80000003 (CPU_SUBTYPE_LIB64|CPU_SUBTYPE_386)
[0xC,0x10) 0x0 filetype=00000002 (MH_EXECUTE)
[0x10,0x14) 0x0 ncmds=0x12
```

It is all very simple plaintext. The three parts are the memory range for a tag, the highlight color for the tag, and finally the text of the tag. Tags are separated by newlines.

Taggers do not have to worry about providing hierarchical information. Hlab will use the values of the memory addresses to assimilate them in a hierarchy and then a TreeView.

I believe this textual tagging approach is superior to tool specific grammars because:
* reading tags is easy, by program or human

Since tags are text, you can use your favorite programming language to generate them. You are not limited by a grammer. You do not have to learn a grammar. The tags are human readable. The tags can be parsed by other utilities easily. Nesting taggers is easy, just append the tag text. For example, an ELF tagger may offload work to a DWARF tagger as it parses.

Hlab will search in ".", "./taggers", and "./usr/local/bin" for any file starting with "hltag_" to invoke as a tagger. A tagger that cannot decompose an input binary should print nothing to stdout and return nonzero. A tagger that is able to decompose should print its tags and return zero.

# Dependencies
* c standard library
* c++ standard template library (vector, map, string)
* my autils library
* re2 (Google's regular expression library)
* llvm 3.8.1
* fltk 1.3.3
