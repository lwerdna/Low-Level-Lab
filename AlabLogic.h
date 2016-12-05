// notes:
// typedef void(Fl_Callback)(Fl_Widget *, void *)

/* GUI lifecycle */
void onGuiFinished(AlabGui *gui);
void onButtonC(void);
void onButtonCPP(void);
void onIdle(void *data);
void onExit(void);

/* GUI triggers for reassemble */
void reassemble(void);
void onExampleSelection(void);
void onConfigSelection(void);
void onDialectChange(void);
void onSourceModified(int pos, int nInserted, int nDeleted, int nRestyled,
    const char * deletedText, void *cbArg);

void onIcCodeModel();
void onIcRelocModel();

void onBtnX86();
void onBtnX86_();
void onBtnX64();
void onBtnX64_();
void onBtnMips();
void onBtnArm();
void onBtnArm64();
void onBtnPpc();
void onBtnPpc64();
void onBtnPpc64le();
void onBtnPpc64le();

void onInpArch();
void onInpVendor();
void onInpOs();
void onInpEnviron();

