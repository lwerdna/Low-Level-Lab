/* c stdlib includes */
#include <time.h>
#include <stdio.h>

/* c++ includes */
#include <string>

/* fltk includes */
#include <FL/Fl.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Display.H>

/* local includes */
#include "AlabGui.h"
#include "rsrc.h"

/* autils */
extern "C" {
#include <autils/crc.h>
#include <autils/filesys.h>
#include <autils/subprocess.h>
}

/* llvm includes */
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCInstPrinter.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCObjectFileInfo.h"
#include "llvm/MC/MCParser/AsmLexer.h"
#define DIALECT_ATT 0
#define DIALECT_INTEL 1
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSectionMachO.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCTargetOptionsCommandFlags.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Compression.h"
#include "llvm/Support/FileUtilities.h"
#include "llvm/Support/FormattedStream.h"

#include "llvm/Support/Host.h" // for getDefaultTargetTriple();

#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/ToolOutputFile.h"

/* globals */

AlabGui *gui = NULL;

/* reassemble state vars */
bool assemble_forced = false;
bool assemble_requested = false;
int length_last_assemble = 0;
time_t time_last_assemble = 0;
uint32_t crc_last_assemble = 0;
/* configuration string */
const char *configTriple = NULL;

/*****************************************************************************/
/* MAIN ROUTINE */
/*****************************************************************************/

void
assemble()
{
    int rc = -1;
    char *srcText = NULL;

    /* assemble timing vars */
    time_t time_now = 0;
    int length_now = 0;
    uint32_t crc_now = 0;

    Fl_Text_Buffer *srcBuf = gui->srcBuf;
    Fl_Text_Buffer *bytesBuf = gui->bytesBuf;

    srcText = srcBuf->text();

    if(!assemble_forced && !assemble_requested) {
        //printf("skipping assemble, neither forced nor requested\n");
        return;
    }
    
    if(!assemble_forced && assemble_requested) {
        /* skip if we assembled within the last second */
        time(&time_now);
        float delta = difftime(time_now, time_last_assemble);
        if(delta >= 0 && delta < 1) {
            /* just too soon, remain requested */
            //printf("skipping assemble, too soon (now,last,diff)=(%ld,%ld,%f)\n",
            //    time_now, time_last_assemble, difftime(time_now, time_last_assemble));
            return;
        }
        else {
            /* skip if the text is unchanged */
            length_now = srcBuf->length();
            if(length_last_assemble == length_now) {
                crc_now = crc32(0, srcText, srcBuf->length());
                if(crc_now == crc_last_assemble) {
                    //printf("skipping assemble, buffer unchanged\n");
                    assemble_requested = false;
                    return;
                }
            }
        }
    }

    assemble_forced = false;
    assemble_requested = false;
    if(crc_now) crc_last_assemble = crc_now;
    if(time_now) time_last_assemble = time_now;
    if(length_now) length_last_assemble = length_now;
    
    /* clear the output text */
    bytesBuf->text("");

    /* real assembly work now */
    SourceMgr SrcMgr;

    if(0) {
        llvm::InitializeAllTargetInfos();
        llvm::InitializeAllTargetMCs();
        llvm::InitializeAllAsmParsers();
        llvm::InitializeAllDisassemblers();
    }
    else {
        LLVMInitializeX86TargetInfo();
        LLVMInitializeX86AsmParser();
        LLVMInitializeX86TargetMC();
        LLVMInitializeX86Disassembler();

        LLVMInitializeARMTargetInfo();
        LLVMInitializeARMAsmParser();
        LLVMInitializeARMTargetMC();
        LLVMInitializeARMDisassembler();

        LLVMInitializeAArch64TargetInfo();
        LLVMInitializeAArch64AsmParser();
        LLVMInitializeAArch64TargetMC();
        LLVMInitializeAArch64Disassembler();

        LLVMInitializePowerPCTargetInfo();
        LLVMInitializePowerPCAsmParser();
        LLVMInitializePowerPCTargetMC();
        LLVMInitializePowerPCDisassembler();
    }

    // arg0:
    // llvm::Target encapsulating the "x86_64-apple-darwin14.5.0" information 

    // see /lib/Support/Triple.cpp for the details
    //spec = llvm::sys::getDefaultTargetTriple();
    std::string machSpec(configTriple);
    //std::string machSpec = "x86_64-apple-darwin";
    //std::string machSpec = "x86_64-thumb-linux-gnu";
    //std::string machSpec = "x86_64-unknown-linux-gnu";
    printf("machine spec: %s\n", machSpec.c_str());
    machSpec = Triple::normalize(machSpec);
    printf("machine spec (normalized): %s\n", machSpec.c_str());
    Triple TheTriple(machSpec);
    printf("triple has been made\n");

    // Get the target specific parser.
    std::string strErr;
    const Target *TheTarget = TargetRegistry::lookupTarget(/*arch*/"", TheTriple, strErr);
    if (!TheTarget) {
        gui->bLog->add("TargetRegistry::lookupTarget() failed");
        gui->bLog->add(strErr.c_str());
        return;
    }

    machSpec = TheTriple.getTriple();
    printf("machine spec (returned): %s\n", machSpec.c_str());
    
    printf("Target.getName(): %s\n", TheTarget->getName());
    printf("Target.getShortDescription(): %s\n", TheTarget->getShortDescription());

    /* from the target we get almost everything */
    std::unique_ptr<MCRegisterInfo> MRI(TheTarget->createMCRegInfo(machSpec));
    std::unique_ptr<MCAsmInfo> MAI(TheTarget->createMCAsmInfo(*MRI, machSpec));
    std::unique_ptr<MCInstrInfo> MCII(TheTarget->createMCInstrInfo()); /* describes target instruction set */
    std::unique_ptr<MCSubtargetInfo> STI(TheTarget->createMCSubtargetInfo(machSpec, "", "")); /* subtarget instr set */
    std::unique_ptr<MCAsmBackend> MAB(TheTarget->createMCAsmBackend(*MRI, machSpec, /* specific CPU */ ""));
    MCInstPrinter *IP =  TheTarget->createMCInstPrinter(Triple(machSpec), /*variant*/0, *MAI, *MCII, *MRI);

    // arg0:
    // llvm::SourceMgr (Support/SourceMgr.h) that holds assembler source
    // has vector of llvm::SrcBuffer encaps (Support/MemoryBuffer.h) and vector of include dirs
    std::string asmSrc(srcText);
    std::unique_ptr<MemoryBuffer> memBuf = MemoryBuffer::getMemBuffer(asmSrc);
    SrcMgr.AddNewSourceBuffer(std::move(memBuf), SMLoc());

    // arg1: the machine code context
    MCObjectFileInfo MOFI;
    MCContext Ctx(MAI.get(), MRI.get(), &MOFI, &SrcMgr);
    MOFI.InitMCObjectFileInfo(TheTriple, /*pic*/ false, CodeModel::Default, Ctx);

    // arg2: the streamer
    //
    // this is the assembler interface
    // -methods per .s statements (emit bytes, handle directive, etc.)
    // -remembers current section
    // -implementations that write a .s, or .o in various formats
    //
    //   1. the output stream ... a formatted_raw_ostream wraps a raw_ostream to provide
    //   tracking of line and column position for padding and shit
    //
    //   but raw_ostream is abstract and is implemented by raw_fd_ostream, raw_string_ostream, etc.
    std::string strOutput;
    raw_string_ostream rso(strOutput);
    formatted_raw_ostream fro(rso);
    std::unique_ptr<formatted_raw_ostream> pfro(&fro);

    /* code emitter needs 1) instruction set info 2) register info */
    MCCodeEmitter *CE = TheTarget->createMCCodeEmitter(*MCII, *MRI, Ctx);

    MCStreamer *as = TheTarget->createAsmStreamer(
        Ctx, /* the MC context */
        std::move(pfro), /* output stream (type: std::unique_ptr<formatted_raw_ostream> from Support/FormattedStream.h) */
        true, /* isVerboseAsm */
        false, /* useDwarfDirectory */
        IP, /* if given, the instruction printer to use (else, MCInstr representation is used) */
        CE, /* if given, a code emitter used to show instruction encoding inline with the asm */
        MAB.get(),  /* the AsmBackend, (fixups, relaxation, objs and elfs) */
        true /* ShowInst (show encoding) */
    );

    std::string abi = "none";
    MCTargetOptions toptions;
    toptions.MCUseDwarfDirectory = false;
    toptions.ABIName = abi;

    printf("trying to assemble, let's go..\n");

    /* create MC target asm parser */
    /* note that the parser will default its dialect to MAI
        (AssemblerInfo)'s dialect if we don't specify */
	std::unique_ptr<MCAsmParser> Parser(
        createMCAsmParser(SrcMgr, Ctx, *as, *MAI)
    );
    if(gui->cbAtt->value()) Parser->setAssemblerDialect(DIALECT_ATT);
    else Parser->setAssemblerDialect(DIALECT_INTEL);

    /* this is TARGET asm parser */
    std::unique_ptr<MCTargetAsmParser> TAP(
        TheTarget->createMCAsmParser(*STI, *Parser, *MCII, toptions)
    );
    if (!TAP) {
        errs() << ": error: this target does not support assembly parsing.\n";
        return;
    }
    Parser->setTargetParser(*TAP);
    int Res = Parser->Run(true);
    if(Res) {
        while(0);
    }

    /* flush the FRO (formatted raw ostream) */
    fro.flush();
    printf("output:\n%s", strOutput.c_str());

    /* output */
    bytesBuf->text(strOutput.c_str());

    rc = 0;
    //cleanup:
    if(srcText) {
        free(srcText);
        srcText = NULL;
    }
    return;
}

/*****************************************************************************/
/* GUI CALLBACK STUFF */
/*****************************************************************************/

/* reassemble request from GUI with 0 args */
void
reassemble(void)
{
    printf("reassemble request\n");
    assemble_forced = true;
}

/* callback when the source code is changed
    (normal callback won't trigger during text change) */
void
onSourceModified(int pos, int nInserted, int nDeleted, int nRestyled,
    const char * deletedText, void *cbArg)
{
    printf("%s()\n", __func__);
    assemble_requested = true;
}

void
onConfigSelection()
{
    configTriple = gui->icPresets->value();
    printf("you selected: %s\n", configTriple);
    //spec = llvm::sys::getDefaultTargetTriple();
    //std::string machSpec = "x86_64-apple-darwin14.5.0";
    //std::string machSpec = "x86_64-apple-darwin";
    //std::string machSpec = "x86_64-thumb-linux-gnu";
    //std::string machSpec = "x86_64-unknown-linux-gnu";
    Triple trip(configTriple);

    std::string arch = trip.getArchName();
    //string subarch = Triple::SubArchType(
    std::string vendor = trip.getVendorName();
    std::string os = trip.getOSName();
    std::string environ = trip.getEnvironmentName();
    Triple::ObjectFormatType objFormat = trip.getObjectFormat();

    gui->oArch->value(arch.c_str());
    gui->oVendor->value(vendor.c_str());
    gui->oOs->value(os.c_str());
    gui->oEnviron->value(environ.c_str());
    switch(objFormat) {
        case Triple::COFF:
            gui->oFormat->value("coff"); break;
        case Triple::ELF:
            gui->oFormat->value("ELF"); break;
        case Triple::MachO:
            gui->oFormat->value("MachO"); break;
        case Triple::UnknownObjectFormat:
            gui->oFormat->value("unknown"); break;
    }

    /* with new choice, reassemble */
    assemble_forced = true;
}

void
onExampleSelection()
{
    const char *file = gui->icExamples->value();

    if(0==strcmp(file, "x86.s")) {
        gui->srcBuf->text((char *)rsrc_x86_s);
        gui->cbAtt->value(1);
    }
    if(0==strcmp(file, "x86_intel.s")) {
        gui->srcBuf->text((char *)rsrc_x86_intel_s);
        gui->cbAtt->value(0);
    }
    else if(0==strcmp(file, "x86_64.s")) {
        gui->cbAtt->value(1);
        gui->srcBuf->text((char *)rsrc_x86_64_s);
    }
    else if(0==strcmp(file, "x86_64_intel.s")) {
        gui->cbAtt->value(0);
        gui->srcBuf->text((char *)rsrc_x86_64_intel_s);
    }
    else if(0==strcmp(file, "ppc32.s")) {
        gui->srcBuf->text((char *)rsrc_ppc_s);
    }
    else if(0==strcmp(file, "ppc64.s")) {
        gui->srcBuf->text((char *)rsrc_ppc64_s);
    }
    else if(0==strcmp(file, "arm.s")) {
        gui->srcBuf->text((char *)rsrc_arm_s);
    }
    else if(0==strcmp(file, "thumb.s")) {
        gui->srcBuf->text((char *)rsrc_thumb_s);
    }
    else if(0==strcmp(file, "arm64.s")) {
        gui->srcBuf->text((char *)rsrc_arm64_s);
    }
}

void
onDialectChange()
{
    int state = gui->cbAtt->value();
    printf("new at&t check state: %d\n", state);
    assemble_forced = true;
}

void
onGuiFinished(AlabGui *gui_)
{
    printf("%s()\n", __func__);
    int rc = -1;

    gui = gui_;

    /* initial input choices */

    gui->icExamples->add("x86_intel.s");
    gui->icExamples->add("x86.s");
    gui->icExamples->add("x86_64_intel.s");
    gui->icExamples->add("x86_64.s");
    gui->icExamples->add("ppc32.s");
    gui->icExamples->add("ppc64.s");
    gui->icExamples->add("arm.s");
    gui->icExamples->add("thumb.s");
    gui->icExamples->add("arm64.s");
    gui->icExamples->value(0);
    gui->cbAtt->value(0);
    onExampleSelection();

    // can test triples with:
    // clang -S -c -target ppc32le-apple-darwin hello.c
    // see also lib/Support/Triple.cpp in llvm source
    // see also http://clang.llvm.org/docs/CrossCompilation.html
    // TODO: add the default machine config */
    gui->icPresets->add("i386-none-none");
    gui->icPresets->add("x86_64-none-none");
    gui->icPresets->add("arm-none-none-eabi");
    gui->icPresets->add("thumb-none-none");
    gui->icPresets->add("aarch64-none-none-eabi");
    gui->icPresets->add("powerpc-none-none");
    gui->icPresets->add("powerpc64-none-none");
    gui->icPresets->add("haha-just-a-joke");
    /* start it at the 0'th value */
    gui->icPresets->value(0);
    /* pretend the user did it */
    onConfigSelection();

    assemble_forced = true;

    rc = 0;
    //cleanup:
    return;
}

void
onIdle(void *data)
{
    assemble();
}

void
onExit(void)
{
    printf("onExit()\n");
}
