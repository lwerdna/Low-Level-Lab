// notes:
// typedef void(Fl_Callback)(Fl_Widget *, void *)

/* GUI lifecycle */
void onGuiFinished(ClabGui *gui);
void onButtonC(void);
void onButtonCPP(void);
void onIdle(void *data);
void onExit(void);

/* GUI triggers for recompile */
void recompile(void);
void onSourceModified(int pos, int nInserted, int nDeleted, int nRestyled,
    const char * deletedText, void *cbArg);



