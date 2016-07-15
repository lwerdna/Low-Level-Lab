// notes:
// typedef void(Fl_Callback)(Fl_Widget *, void *)

void compile(Gui *gui);

void onSourceModified(int pos, int nInserted, int nDeleted, int nRestyled,
    const char * deletedText, void *cbArg);

void onGuiFinished(Gui *gui);

void onIdle(void *data);
void onExit(void);

void onClangSettingsChange(Fl_Widget *, void *);

void onCustomFlags(Fl_Widget *, void *);

void onOutputWrap(Fl_Widget *, void *);
void onOutputScroll(Fl_Widget *, void *);
