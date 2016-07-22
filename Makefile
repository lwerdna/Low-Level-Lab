CXX      = g++
DEBUG    = -g -O0
CXXFLAGS = $(shell fltk-config --use-images --cxxflags ) -I.
LDFLAGS  = $(shell fltk-config --use-images --ldflags )
LDSTATIC = $(shell fltk-config --use-images --ldstaticflags ) -lautils
LINK     = $(CXX)

TARGET = clab
OBJS_COMMON = Fl_Text_Editor_C.o Fl_Text_Editor_Asm.o
OBJS_CLAB = $(OBJS_COMMON) ClabGui.o ClabLogic.o 
OBJS_ALAB = $(OBJS_COMMON) AlabGui.o AlabLogic.o 
SRCS = ClabGui.cxx ClabLogic.cxx Fl_Text_Editor_C.cxx Fl_Text_Editor_Asm.cxx

# this is a pattern rule
.SUFFIXES: .o .cxx
%.o: %.cxx
	$(CXX) $(CXXFLAGS) $(DEBUG) -c $<

all: clab

clab: $(OBJS_CLAB) Makefile
	$(LINK) -o clab $(LDDFLAGS) $(OBJS_CLAB) $(LDSTATIC)

clean: $(TARGET) $(OBJS)
	rm -f *.o 2> /dev/null
	rm -f $(TARGET) 2> /dev/null

install:
	install ./clab /usr/local/bin
