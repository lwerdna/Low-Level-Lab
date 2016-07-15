void compile(Gui *gui);

void onSourceModified(int pos, int nInserted, int nDeleted, int nRestyled,
    const char * deletedText, void *cbArg);

void onGuiFinished(Gui *gui);

void onIdle(void *data);
void onExit(void);

