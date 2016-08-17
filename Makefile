FLAGS_DEBUG = -g -O0
FLAGS_LLVM = $(shell llvm-config --cxxflags)
FLAGS_FLTK = $(shell fltk-config --use-images --cxxflags )
#LDFLAGS  = $(shell fltk-config --use-images --ldflags )
LD_FLTK = $(shell fltk-config --use-images --ldstaticflags)
LD_LLVM = $(shell llvm-config --ldflags) $(shell llvm-config --libs)
LINK = $(CXX)

# this is a pattern rule
#.SUFFIXES: .o .cxx
#%.o: %.cxx
#	$(CXX) $(CXXFLAGS) $(DEBUG) -c $<

all: clab alab hlab

# GUI objects
#
Fl_Text_Editor_C.o: Fl_Text_Editor_C.cxx Fl_Text_Editor_C.h
	g++ $(FLAGS_FLTK) $(FLAGS_DEBUG) -c Fl_Text_Editor_C.cxx

Fl_Text_Editor_Asm.o: Fl_Text_Editor_Asm.cxx Fl_Text_Editor_Asm.h
	g++ $(FLAGS_FLTK) $(FLAGS_DEBUG) -c Fl_Text_Editor_Asm.cxx

Fl_Text_Display_Log.o: Fl_Text_Display_Log.cxx Fl_Text_Display_Log.h
	g++ $(FLAGS_FLTK) $(FLAGS_DEBUG) -c Fl_Text_Display_Log.cxx

HexView.o: HexView.cxx HexView.h
	g++ $(FLAGS_FLTK) $(FLAGS_DEBUG) -c HexView.cxx

ClabGui.o: ClabGui.cxx ClabGui.h
	g++ $(FLAGS_FLTK) $(FLAGS_DEBUG) -c ClabGui.cxx

HlabGui.o: HlabGui.cxx HlabGui.h
	g++ $(FLAGS_FLTK) $(FLAGS_DEBUG) -c HlabGui.cxx

AlabGui.o: AlabGui.cxx AlabGui.h
	g++ $(FLAGS_FLTK) $(FLAGS_DEBUG) -c AlabGui.cxx

# LOGIC objects
#
ClabLogic.o: ClabLogic.cxx ClabLogic.h
	g++ $(FLAGS_FLTK) $(FLAGS_DEBUG) -c ClabLogic.cxx

AlabLogic.o: AlabLogic.cxx AlabLogic.h
	g++ $(FLAGS_FLTK) $(FLAGS_DEBUG) -c AlabLogic.cxx

HlabLogic.o: HlabLogic.cxx HlabLogic.h
	g++ $(FLAGS_FLTK) $(FLAGS_DEBUG) -c HlabLogic.cxx

# OTHER objects
llvm_svcs.o: llvm_svcs.cxx llvm_svcs.h
	g++ $(FLAGS_LLVM) $(FLAGS_DEBUG) -c llvm_svcs.cxx

# RESOURCES
rsrc.c: ./rsrc/arm.s ./rsrc/arm64.s ./rsrc/mips.s ./rsrc/ppc.s ./rsrc/thumb.s ./rsrc/x86.s ./rsrc/x86_64.s ./rsrc/x86_intel.s ./rsrc/x86_64_intel.s
	./genrsrc.py source > rsrc.c
	
rsrc.h: ./rsrc/arm.s ./rsrc/arm64.s ./rsrc/mips.s ./rsrc/ppc.s ./rsrc/thumb.s ./rsrc/x86.s ./rsrc/x86_64.s ./rsrc/x86_intel.s ./rsrc/x86_64_intel.s
	./genrsrc.py header > rsrc.h

rsrc.o: rsrc.c rsrc.h
	gcc -c rsrc.c

# LINK
#
clab: ClabGui.o ClabLogic.o Fl_Text_Editor_C.o Fl_Text_Editor_Asm.o Makefile
	$(LINK) ClabGui.o ClabLogic.o Fl_Text_Editor_C.o Fl_Text_Editor_Asm.o -o clab $(LD_FLTK) -lautils

alab: rsrc.o AlabGui.o AlabLogic.o llvm_svcs.o Fl_Text_Editor_Asm.o Fl_Text_Display_Log.o Makefile
	$(LINK) AlabGui.o AlabLogic.o llvm_svcs.o Fl_Text_Editor_Asm.o Fl_Text_Display_log.o rsrc.o -o alab $(LD_FLTK) $(LD_LLVM)

hlab: HlabGui.o HlabLogic.o HexView.o Makefile
	$(LINK) HlabGui.o HlabLogic.o HexView.o -o hlab $(LD_FLTK) -lautils

# OTHER targets
#
clean: $(TARGET) $(OBJS)
	rm -f *.o 2> /dev/null
	rm -f $(TARGET) 2> /dev/null
	rm alab clab hlab rsrc.c rsrc.h 2> /dev/null

install:
	install ./clab /usr/local/bin
	install ./alab /usr/local/bin

