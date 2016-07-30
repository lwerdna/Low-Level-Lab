// generated by Fast Light User Interface Designer (fluid) version 1.0303

#ifndef AlabGui_h
#define AlabGui_h
#include <FL/Fl.H>
#include "Fl_Text_Editor_Asm.h"
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_Input_Choice.H>

class AlabGui {
public:
  Fl_Double_Window* make_window();
  Fl_Text_Editor_Asm *srcCode;
  Fl_Text_Editor *asmCode;
  Fl_Output *oArch;
  Fl_Output *oSubArch;
  Fl_Output *oOs;
  Fl_Output *oEnviron;
  Fl_Output *oFormat;
  Fl_Input_Choice *icPresets;
private:
  inline void cb_icPresets_i(Fl_Input_Choice*, void*);
  static void cb_icPresets(Fl_Input_Choice*, void*);
public:
  Fl_Output *oVendor;
  Fl_Input_Choice *icExamples;
private:
  inline void cb_icExamples_i(Fl_Input_Choice*, void*);
  static void cb_icExamples(Fl_Input_Choice*, void*);
public:
  Fl_Text_Buffer *srcBuf; 
  Fl_Text_Buffer *bytesBuf; 
};
#endif
