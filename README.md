Low Level Laboratory

# Compiler LAB (clab)
* a little GUI to watch your compilers generate code

# Assembler LAB (alab)
* a little GUI that assembles for various architectures by calling into llvm libs

# Hex LAB (hlab)
* hex editor with "decompose" functionality with a tree view
* decomposers take a different approach: python scripts that simply tag or mark regions of memory with names by printing out text (like "[0,0x40] Elf64_Header")
* hlab parses the text and takes care of region containment and other issues 

# Dependencies
* c standard library
* c++ standard template library (vector, map, string)
* my autils library
* llvm 3.8.1
* fltk 1.3.3
