SHELL = /bin/bash
CFLAGS = -std=c++11
FLAGS_DEBUG = -g -O0
FLAGS_LINK = -L/usr/local/lib
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

all: clab alab hlab test

# GUI objects
#
Fl_Text_Editor_C.o: Fl_Text_Editor_C.cxx Fl_Text_Editor_C.h
	g++ $(CFLAGS) $(FLAGS_FLTK) $(FLAGS_DEBUG) -c Fl_Text_Editor_C.cxx

Fl_Text_Editor_Asm.o: Fl_Text_Editor_Asm.cxx Fl_Text_Editor_Asm.h
	g++ $(CFLAGS) $(FLAGS_FLTK) $(FLAGS_DEBUG) -c Fl_Text_Editor_Asm.cxx

Fl_Text_Display_Log.o: Fl_Text_Display_Log.cxx Fl_Text_Display_Log.h
	g++ $(CFLAGS) $(FLAGS_FLTK) $(FLAGS_DEBUG) -c Fl_Text_Display_Log.cxx

HexView.o: HexView.cxx HexView.h
	g++ $(CFLAGS) $(FLAGS_FLTK) $(FLAGS_DEBUG) -c HexView.cxx

ClabGui.o: ClabGui.cxx ClabGui.h
	g++ $(CFLAGS) $(FLAGS_FLTK) $(FLAGS_DEBUG) -c ClabGui.cxx

HlabGui.o: HlabGui.cxx HlabGui.h
	g++ $(CFLAGS) $(FLAGS_FLTK) $(FLAGS_DEBUG) -c HlabGui.cxx

AlabGui.o: AlabGui.cxx AlabGui.h
	g++ $(CFLAGS) $(FLAGS_FLTK) $(FLAGS_DEBUG) -c AlabGui.cxx

# LOGIC objects
#
ClabLogic.o: ClabLogic.cxx ClabLogic.h
	g++ $(CFLAGS) $(FLAGS_FLTK) $(FLAGS_DEBUG) -c ClabLogic.cxx

AlabLogic.o: AlabLogic.cxx AlabLogic.h
	g++ $(CFLAGS) $(FLAGS_FLTK) $(FLAGS_DEBUG) -c AlabLogic.cxx

HlabLogic.o: HlabLogic.cxx HlabLogic.h
	g++ $(CFLAGS) $(FLAGS_FLTK) $(FLAGS_DEBUG) -c HlabLogic.cxx

# OTHER objects
llvm_svcs.o: llvm_svcs.cxx llvm_svcs.h
	g++ $(CFLAGS) $(FLAGS_LLVM) $(FLAGS_DEBUG) -c llvm_svcs.cxx

tagging.o: tagging.cxx tagging.h
	g++ $(CFLAGS) $(FLAGS_DEBUG) -c tagging.cxx

IntervalMgr.o: IntervalMgr.cxx IntervalMgr.h
	g++ $(CFLAGS) $(FLAGS_DEBUG) -c IntervalMgr.cxx

test.o: test.cpp
	g++ $(CFLAGS) $(FLAGS_DEBUG) -c test.cpp

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
	$(LINK) $(FLAGS_LINK) ClabGui.o ClabLogic.o Fl_Text_Editor_C.o Fl_Text_Editor_Asm.o -o clab $(LD_FLTK) -lautils

alab: rsrc.o AlabGui.o AlabLogic.o IntervalMgr.o llvm_svcs.o Fl_Text_Editor_Asm.o Fl_Text_Display_Log.o HexView.o Makefile
	$(LINK)  $(FLAGS_LINK) AlabGui.o AlabLogic.o llvm_svcs.o Fl_Text_Editor_Asm.o Fl_Text_Display_log.o HexView.o IntervalMgr.o rsrc.o -o alab $(LD_FLTK) $(LD_LLVM) -lautils -lre2

hlab: HlabGui.o HlabLogic.o HexView.o IntervalMgr.o tagging.o Makefile
	$(LINK)  $(FLAGS_LINK) HlabGui.o HlabLogic.o HexView.o IntervalMgr.o tagging.o -o hlab $(LD_FLTK) -lautils -lre2

test: test.o tagging.o IntervalMgr.o
	$(LINK) $(FLAGS_LINK) test.o tagging.o IntervalMgr.o -lautils -lre2 -o test

# OTHER targets
#
clean: $(TARGET) $(OBJS)
	rm -f *.o 2> /dev/null
	rm -f $(TARGET) 2> /dev/null
	rm alab clab hlab rsrc.c rsrc.h 2> /dev/null

install:
	install ./clab /usr/local/bin
	install ./alab /usr/local/bin
	install ./hlab /usr/local/bin
	install ./taggers/* /usr/local/bin

uninstall:
	if [ -f "/usr/local/bin/clab" ]; then rm /usr/local/bin/clab; fi
	if [ -f "/usr/local/bin/alab" ]; then rm /usr/local/bin/alab; fi
	if [ -f "/usr/local/bin/hlab" ]; then rm /usr/local/bin/hlab; fi
	rm /usr/local/bin/hltag_*
