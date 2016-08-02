FLAGS_DEBUG = -g -O0
FLAGS_LLVM = $(shell llvm-config --cxxflags) -fno-rtti
FLAGS_FLTK = $(shell fltk-config --use-images --cxxflags )
#LDFLAGS  = $(shell fltk-config --use-images --ldflags )
LD_FLTK = $(shell fltk-config --use-images --ldstaticflags ) -lautils
LD_LLVM = -lLLVMSupport \
    -lLLVMMC -lLLVMMCParser -lLLVMMCDisassembler \
    -lLLVMAsmParser -lLLVMAsmPrinter \
    -lLLVMX86AsmParser -lLLVMX86CodeGen -lLLVMX86Disassembler -lLLVMX86Utils \
    -lLLVMX86AsmPrinter -lLLVMX86Desc -lLLVMX86Info \
    -lLLVMArmAsmParser -lLLVMArmCodeGen -lLLVMArmDisassembler \
    -lLLVMArmAsmPrinter -lLLVMArmDesc -lLLVMArmInfo \
    -lLLVMAArch64AsmParser -lLLVMAArch64CodeGen -lLLVMAArch64Disassembler -lLLVMAArch64Utils \
    -lLLVMAArch64AsmPrinter -lLLVMAArch64Desc -lLLVMAArch64Info \
    -lLLVMPowerPCAsmParser -lLLVMPowerPCCodeGen -lLLVMPowerPCDisassembler \
    -lLLVMPowerPCAsmPrinter -lLLVMPowerPCDesc -lLLVMPowerPCInfo \
    -lncurses
LINK = $(CXX)

# this is a pattern rule
#.SUFFIXES: .o .cxx
#%.o: %.cxx
#	$(CXX) $(CXXFLAGS) $(DEBUG) -c $<

all: clab alab

# GUI objects
#
Fl_Text_Editor_C.o: Fl_Text_Editor_C.cxx Fl_Text_Editor_C.h
	g++ $(FLAGS_FLTK) $(FLAGS_DEBUG) -c Fl_Text_Editor_C.cxx

Fl_Text_Editor_Asm.o: Fl_Text_Editor_Asm.cxx Fl_Text_Editor_Asm.h
	g++ $(FLAGS_FLTK) $(FLAGS_DEBUG) -c Fl_Text_Editor_Asm.cxx

ClabGui.o: ClabGui.cxx ClabGui.h
	g++ $(FLAGS_FLTK) $(FLAGS_DEBUG) -c ClabGui.cxx

AlabGui.o: AlabGui.cxx AlabGui.h
	g++ $(FLAGS_FLTK) $(FLAGS_DEBUG) -c AlabGui.cxx

# LOGIC objects
#
ClabLogic.o: ClabLogic.cxx ClabLogic.h
	g++ $(FLAGS_FLTK) $(FLAGS_DEBUG) -c ClabLogic.cxx

AlabLogic.o: AlabLogic.cxx AlabLogic.h
	g++ $(FLAGS_FLTK) $(FLAGS_LLVM) $(FLAGS_DEBUG) -c AlabLogic.cxx

# RESOURCES
rsrc.c: ./rsrc/arm.s ./rsrc/arm64.s ./rsrc/mips.s ./rsrc/ppc.s ./rsrc/thumb.s ./rsrc/x86.s ./rsrc/x86_64.s
	./genrsrc.py source > rsrc.c
	./genrsrc.py header > rsrc.h

rsrc.o: rsrc.c
	gcc -c rsrc.c

# LINK
#
clab: ClabGui.o ClabLogic.o Fl_Text_Editor_C.o Fl_Text_Editor_Asm.o Makefile
	$(LINK) ClabGui.o ClabLogic.o Fl_Text_Editor_C.o Fl_Text_Editor_Asm.o -o clab $(LD_FLTK)

alab: AlabGui.o AlabLogic.o Fl_Text_Editor_Asm.o rsrc.o Makefile
	$(LINK) AlabGui.o AlabLogic.o Fl_Text_Editor_Asm.o rsrc.o -o alab $(LD_FLTK) $(LD_LLVM)

# OTHER targets
#
clean: $(TARGET) $(OBJS)
	rm -f *.o 2> /dev/null
	rm -f $(TARGET) 2> /dev/null
	rm alab clab rsrc.c rsrc.h

install:
	install ./clab /usr/local/bin
	install ./alab /usr/local/bin

