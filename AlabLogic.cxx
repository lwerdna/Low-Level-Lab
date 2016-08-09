/* c stdlib includes */
#include <time.h>
#include <stdio.h>

/* c++ includes */
#include <map>
#include <string>
#include <vector>
using namespace std;

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
#include <autils/output.h>
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


/* source manager diagnostics handler
    (instead of printing to stderr) */
void srcMgrDiagHndlr(const SMDiagnostic &diag, void *ctx)
{
    char buf[128];
    //printf("Source Manager Diagnostic Handler:\n");
 
    switch(diag.getKind()) {
        case SourceMgr::DK_Note:
            gui->log->setColor(FL_WHITE);
            gui->log->append("NOTE "); 
            break;
        case SourceMgr::DK_Warning:
            gui->log->setColor(FL_YELLOW);
            gui->log->append("WARNING ");
            break;
        case SourceMgr::DK_Error:
            gui->log->setColor(FL_RED);
            gui->log->append("ERROR "); 
            break;
    }

    std::string fName(diag.getFilename());
    std::string message(diag.getMessage());
   
    gui->log->setColor(FL_WHITE);
    sprintf(buf, "line %d: ", diag.getLineNo());
    gui->log->append(buf);
    gui->log->append(message.c_str());
    sprintf(buf, "\n");
    gui->log->append(buf);
}

/*****************************************************************************/
/* MAIN ROUTINE */
/*****************************************************************************/


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

    /* clear the log output */
    gui->log->clear(); 

    /* real assembly work now */
    SourceMgr SrcMgr;
    SrcMgr.setDiagHandler(srcMgrDiagHndlr, nullptr);

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
        gui->log->append("TargetRegistry::lookupTarget() failed");
        gui->log->append(strErr.c_str());
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

    /* parse the output into hex bytes */
    {
        string result;
        map<string,unsigned char> lookup = {
            {"0x00",'\x00'}, {"0x01",'\x01'}, {"0x02",'\x02'}, {"0x03",'\x03'}, {"0x04",'\x04'}, {"0x05",'\x05'}, {"0x06",'\x06'}, {"0x07",'\x07'},
            {"0x08",'\x08'}, {"0x09",'\x09'}, {"0x0a",'\x0a'}, {"0x0b",'\x0b'}, {"0x0c",'\x0c'}, {"0x0d",'\x0d'}, {"0x0e",'\x0e'}, {"0x0f",'\x0f'},
            {"0x10",'\x10'}, {"0x11",'\x11'}, {"0x12",'\x12'}, {"0x13",'\x13'}, {"0x14",'\x14'}, {"0x15",'\x15'}, {"0x16",'\x16'}, {"0x17",'\x17'},
            {"0x18",'\x18'}, {"0x19",'\x19'}, {"0x1a",'\x1a'}, {"0x1b",'\x1b'}, {"0x1c",'\x1c'}, {"0x1d",'\x1d'}, {"0x1e",'\x1e'}, {"0x1f",'\x1f'},
            {"0x20",'\x20'}, {"0x21",'\x21'}, {"0x22",'\x22'}, {"0x23",'\x23'}, {"0x24",'\x24'}, {"0x25",'\x25'}, {"0x26",'\x26'}, {"0x27",'\x27'},
            {"0x28",'\x28'}, {"0x29",'\x29'}, {"0x2a",'\x2a'}, {"0x2b",'\x2b'}, {"0x2c",'\x2c'}, {"0x2d",'\x2d'}, {"0x2e",'\x2e'}, {"0x2f",'\x2f'},
            {"0x30",'\x30'}, {"0x31",'\x31'}, {"0x32",'\x32'}, {"0x33",'\x33'}, {"0x34",'\x34'}, {"0x35",'\x35'}, {"0x36",'\x36'}, {"0x37",'\x37'},
            {"0x38",'\x38'}, {"0x39",'\x39'}, {"0x3a",'\x3a'}, {"0x3b",'\x3b'}, {"0x3c",'\x3c'}, {"0x3d",'\x3d'}, {"0x3e",'\x3e'}, {"0x3f",'\x3f'},
            {"0x40",'\x40'}, {"0x41",'\x41'}, {"0x42",'\x42'}, {"0x43",'\x43'}, {"0x44",'\x44'}, {"0x45",'\x45'}, {"0x46",'\x46'}, {"0x47",'\x47'},
            {"0x48",'\x48'}, {"0x49",'\x49'}, {"0x4a",'\x4a'}, {"0x4b",'\x4b'}, {"0x4c",'\x4c'}, {"0x4d",'\x4d'}, {"0x4e",'\x4e'}, {"0x4f",'\x4f'},
            {"0x50",'\x50'}, {"0x51",'\x51'}, {"0x52",'\x52'}, {"0x53",'\x53'}, {"0x54",'\x54'}, {"0xZ5",'\x55'}, {"0x56",'\x56'}, {"0x57",'\x57'},
            {"0x58",'\x58'}, {"0x59",'\x59'}, {"0x5a",'\x5a'}, {"0x5b",'\x5b'}, {"0x5c",'\x5c'}, {"0x5d",'\x5d'}, {"0x5e",'\x5e'}, {"0x5f",'\x5f'},
            {"0x60",'\x60'}, {"0x61",'\x61'}, {"0x62",'\x62'}, {"0x63",'\x63'}, {"0x64",'\x64'}, {"0x65",'\x65'}, {"0x66",'\x66'}, {"0x67",'\x67'},
            {"0x68",'\x68'}, {"0x69",'\x69'}, {"0x6a",'\x6a'}, {"0x6b",'\x6b'}, {"0x6c",'\x6c'}, {"0x6d",'\x6d'}, {"0x6e",'\x6e'}, {"0x6f",'\x6f'},
            {"0x70",'\x70'}, {"0x71",'\x71'}, {"0x72",'\x72'}, {"0x73",'\x73'}, {"0x74",'\x74'}, {"0x75",'\x75'}, {"0x76",'\x76'}, {"0x77",'\x77'},
            {"0x78",'\x78'}, {"0x79",'\x79'}, {"0x7a",'\x7a'}, {"0x7b",'\x7b'}, {"0x7c",'\x7c'}, {"0x7d",'\x7d'}, {"0x7e",'\x7e'}, {"0x7f",'\x7f'},
            {"0x80",'\x80'}, {"0x81",'\x81'}, {"0x82",'\x82'}, {"0x83",'\x83'}, {"0x84",'\x84'}, {"0x85",'\x85'}, {"0x86",'\x86'}, {"0x87",'\x87'},
            {"0x88",'\x88'}, {"0x89",'\x89'}, {"0x8a",'\x8a'}, {"0x8b",'\x8b'}, {"0x8c",'\x8c'}, {"0x8d",'\x8d'}, {"0x8e",'\x8e'}, {"0x8f",'\x8f'},
            {"0x90",'\x90'}, {"0x91",'\x91'}, {"0x92",'\x92'}, {"0x93",'\x93'}, {"0x94",'\x94'}, {"0x95",'\x95'}, {"0x96",'\x96'}, {"0x97",'\x97'},
            {"0x98",'\x98'}, {"0x99",'\x99'}, {"0x9a",'\x9a'}, {"0x9b",'\x9b'}, {"0x9c",'\x9c'}, {"0x9d",'\x9d'}, {"0x9e",'\x9e'}, {"0x9f",'\x9f'},
            {"0xa0",'\xa0'}, {"0xa1",'\xa1'}, {"0xa2",'\xa2'}, {"0xa3",'\xa3'}, {"0xa4",'\xa4'}, {"0xa5",'\xa5'}, {"0xa6",'\xa6'}, {"0xa7",'\xa7'},
            {"0xa8",'\xa8'}, {"0xa9",'\xa9'}, {"0xaa",'\xaa'}, {"0xab",'\xab'}, {"0xac",'\xac'}, {"0xad",'\xad'}, {"0xae",'\xae'}, {"0xaf",'\xaf'},
            {"0xb0",'\xb0'}, {"0xb1",'\xb1'}, {"0xb2",'\xb2'}, {"0xb3",'\xb3'}, {"0xb4",'\xb4'}, {"0xb5",'\xb5'}, {"0xb6",'\xb6'}, {"0xb7",'\xb7'},
            {"0xb8",'\xb8'}, {"0xb9",'\xb9'}, {"0xba",'\xba'}, {"0xbb",'\xbb'}, {"0xbc",'\xbc'}, {"0xbd",'\xbd'}, {"0xbe",'\xbe'}, {"0xbf",'\xbf'},
            {"0xc0",'\xc0'}, {"0xc1",'\xc1'}, {"0xc2",'\xc2'}, {"0xc3",'\xc3'}, {"0xc4",'\xc4'}, {"0xc5",'\xc5'}, {"0xc6",'\xc6'}, {"0xc7",'\xc7'},
            {"0xc8",'\xc8'}, {"0xc9",'\xc9'}, {"0xca",'\xca'}, {"0xcb",'\xcb'}, {"0xcc",'\xcc'}, {"0xcd",'\xcd'}, {"0xce",'\xce'}, {"0xcf",'\xcf'},
            {"0xd0",'\xd0'}, {"0xd1",'\xd1'}, {"0xd2",'\xd2'}, {"0xd3",'\xd3'}, {"0xd4",'\xd4'}, {"0xd5",'\xd5'}, {"0xd6",'\xd6'}, {"0xd7",'\xd7'},
            {"0xd8",'\xd8'}, {"0xd9",'\xd9'}, {"0xda",'\xda'}, {"0xdb",'\xdb'}, {"0xdc",'\xdc'}, {"0xdd",'\xdd'}, {"0xde",'\xde'}, {"0xdf",'\xdf'},
            {"0xe0",'\xe0'}, {"0xe1",'\xe1'}, {"0xe2",'\xe2'}, {"0xe3",'\xe3'}, {"0xe4",'\xe4'}, {"0xe5",'\xe5'}, {"0xe6",'\xe6'}, {"0xe7",'\xe7'},
            {"0xe8",'\xe8'}, {"0xe9",'\xe9'}, {"0xea",'\xea'}, {"0xeb",'\xeb'}, {"0xec",'\xec'}, {"0xed",'\xed'}, {"0xee",'\xee'}, {"0xef",'\xef'},
            {"0xf0",'\xf0'}, {"0xf1",'\xf1'}, {"0xf2",'\xf2'}, {"0xf3",'\xf3'}, {"0xf4",'\xf4'}, {"0xf5",'\xf5'}, {"0xf6",'\xf6'}, {"0xf7",'\xf7'},
            {"0xf8",'\xf8'}, {"0xf9",'\xf9'}, {"0xfa",'\xfa'}, {"0xfb",'\xfb'}, {"0xfc",'\xfc'}, {"0xfd",'\xfd'}, {"0xfe",'\xfe'}, {"0xff",'\xff'}
        };
        const char *cur = strOutput.c_str();
        const char *stop = cur+strlen(cur);
        for(; cur<stop; cur++) {
            if(0==strncmp(cur,"# encoding: [", 13)) {
                cur += 13;
                while(1) {
                    if(0!=strncmp(cur,"0x",2)) { printf("expected: 0x\n"); break; }
                    string hexStr = string(cur,4);
                    if(lookup.find(hexStr) == lookup.end()) { printf("unknown byte: %s\n", hexStr.c_str()); break; }
                    result += lookup[hexStr];
                    //printf("extracted: %s\n", hexStr.c_str());
                    cur += 4;
                    if(*cur==']') break;
                    if(*cur!=',') { printf("ERROR: expected ,\n"); break; }
                    cur++;
                }
            }
        }

        // now print hex dump
        unsigned char *tmp = (unsigned char *)result.data();
        dump_bytes(tmp, result.length(), 0);
    }
                

    //printf("output:\n%s", strOutput.c_str());

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