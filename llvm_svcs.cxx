/* abstract away LLVM details from the rest of LLL */

/* c++ includes */
#include <map>
#include <string>
#include <vector>
using namespace std;

/* llvm includes */
#include <llvm/MC/MCAsmBackend.h>
#include <llvm/MC/MCAsmInfo.h>
#include <llvm/MC/MCContext.h>
#include <llvm/MC/MCInstPrinter.h>
#include <llvm/MC/MCInstrInfo.h>
#include <llvm/MC/MCObjectFileInfo.h>
#include <llvm/MC/MCParser/AsmLexer.h>
#define DIALECT_ATT 0
#define DIALECT_INTEL 1

/* note that at least in 4.0.0 and onward, this gets moved to llvm/MC/MCParser/ */
#include <llvm/MC/MCTargetAsmParser.h>
#include <llvm/MC/MCRegisterInfo.h>
#include <llvm/MC/MCSectionMachO.h>
#include <llvm/MC/MCStreamer.h>
#include <llvm/MC/MCSubtargetInfo.h>
#include <llvm/MC/MCTargetOptionsCommandFlags.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Compression.h>
#include <llvm/Support/FileUtilities.h>
#include <llvm/Support/FormattedStream.h>

#include <llvm/Support/Host.h> // for getDefaultTargetTriple();

#include <llvm/Support/ManagedStatic.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/Signals.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/ToolOutputFile.h>

/* autils */
extern "C" {
#include <autils/output.h>
}

/* local includes */
#include "llvm_svcs.h"

/* source manager diagnostics handler
	(instead of printing to stderr) */
/* we set LLVM's callback to this thunk which then calls the user
	requested callback, hiding LLVM details */
static 
void
diag_cb(const SMDiagnostic &diag, void *param)
{
	if(!param) return;

	#ifdef __GNUC__
	__extension__
	#endif
	llvm_svcs_assemble_cb_type pfunc = (llvm_svcs_assemble_cb_type)param;

	int k;
	switch(diag.getKind()) {
		case SourceMgr::DK_Note: k=LLVM_SVCS_CB_NOTE; break;
		case SourceMgr::DK_Warning: k=LLVM_SVCS_CB_WARNING; break;
		case SourceMgr::DK_Error: k=LLVM_SVCS_CB_ERROR; break;
	}

	// call back back to llvm_svcs user
	string fileName = diag.getFilename().str();
	string message = diag.getMessage().str();
	pfunc(k, fileName.c_str(), diag.getLineNo(), message.c_str()); 
}

/* map LLVM services code model to LLVM code model
	(so API users don't have to include LLVM headers) */
CodeModel::Model
map_code_model(int codeModel)
{
	/* SEE: include/llvm-c/TargetMachine.h */
	switch(codeModel) {
		case LLVM_SVCS_CM_DEFAULT: return CodeModel::JITDefault;
		case LLVM_SVCS_CM_SMALL: return CodeModel::Small;
		case LLVM_SVCS_CM_KERNEL: return CodeModel::Kernel;
		case LLVM_SVCS_CM_MEDIUM: return CodeModel::Medium;
		case LLVM_SVCS_CM_LARGE: return CodeModel::Large;
		default: return CodeModel::JITDefault;
	}
}

/* map LLVM services reloc mode to LLVM reloc mode
	(so API users don't have to include LLVM headers) */
Reloc::Model
map_reloc_mode(int relocMode)
{
	/* SEE: include/llvm-c/TargetMachine.h */
	switch(relocMode) {
		case LLVM_SVCS_RM_DEFAULT: return Reloc::Default;
		case LLVM_SVCS_RM_STATIC: return Reloc::Static;
		case LLVM_SVCS_RM_PIC: return Reloc::PIC_;
		case LLVM_SVCS_RM_DYNAMIC_NO_PIC: return Reloc::DynamicNoPIC;
		default: return Reloc::Default;
	}
}

int
asm_output_to_instr_counts(const char *asmText, vector<int> &result)
{
	int rc = -1;
	result.clear();

	const char *cur = asmText;
	const char *stop = cur+strlen(asmText);

	for(; cur<stop; cur++) {
		/* if an encoding block */
		if(0==strncmp(cur,"# encoding: [", 13)) {
			cur += 13;

			int instrSize = 0;

			while(1) {
				/* parse byte or placeholder */
				if(0 == strncmp(cur,"0x", 2)) {
					cur += 4; /* skip 0x?? */
				}
				else if(cur[0]=='A') {
					cur += 1; /* skip A */
				}
				else {
					//printf("ERROR: expected a 0x?? format byte or A placeholder\n");
					goto cleanup;
				}

				/* increment instruction size */
				instrSize += 1;

				/* is encoding block over? */
				if(*cur==']') {
					break;
				} else if(*cur==',') {
					cur++;
				} else {
					//printf("ERROR: expected a ']' or ',' after byte\n");
					goto cleanup;
				}
			}

			result.push_back(instrSize);
		}
	}

	rc = 0;
	cleanup:
	return rc;
}

int
obj_output_to_bytes(const char *data, string &result)
{
	int rc = -1;

	int codeOffset=0, codeSize = 0;
	if(*(uint32_t *)data == 0xFEEDFACF) {
		unsigned int idx = 0;
		idx += 0x20; /* skip mach_header_64 to first command */
		idx += 0x48; /* advance into segment_command_64 to first section */
		idx += 0x28; /* advance into section_64 to size */
		uint64_t scn_size = *(uint64_t *)(data + idx);
		idx += 0x8; /* advance into section_64 to offset */
		uint64_t scn_offset = *(uint64_t *)(data + idx);
		codeOffset = scn_offset;
		codeSize = scn_size;
	}
	else if(0==memcmp(data, "\x7F" "ELF\x01\x01\x01\x00", 8)) {
		/* assume four sections: NULL, .strtab, .text, .symtab */
		uint32_t e_shoff = *(uint32_t *)(data + 0x20);
		uint32_t sh_offset = *(uint32_t *)(data + e_shoff + 2*0x28 + 0x10); /* second shdr */
		uint32_t sh_size = *(uint32_t *)(data + e_shoff + 2*0x28 + 0x14); /* second shdr */
		codeOffset = sh_offset;
		codeSize = sh_size;
	}
	else {
		printf("ERROR: couldn't identify type of output file\n");
		goto cleanup;
	}

	result.assign(reinterpret_cast<char const *>(data+codeOffset), codeSize);

	rc = 0;
	cleanup:
	return rc;
}

int
invoke_llvm_parsers(const Target *TheTarget, SourceMgr *SrcMgr, MCContext &context, 
	MCStreamer &Str, MCAsmInfo &MAI, MCSubtargetInfo &STI, MCInstrInfo &MCII, 
	MCTargetOptions &MCOptions, int dialect)
{
	int rc = -1;

	/* create a vanilla (non-target) AsmParser (lib/MC/MCParser/AsmParser.cpp) */
	std::unique_ptr<MCAsmParser> Parser(createMCAsmParser(*SrcMgr, context, Str, MAI));

	/* set the dialect (otherwise it defaults to assemblerInfo's dialect) */
	switch(dialect) {
		case LLVM_SVCS_DIALECT_UNSPEC:
			break;
		case LLVM_SVCS_DIALECT_ATT:
			Parser->setAssemblerDialect(DIALECT_ATT);
			break;
		case LLVM_SVCS_DIALECT_INTEL:
			Parser->setAssemblerDialect(DIALECT_INTEL);
			break;
	}

	/* TARGET asm parser */
  	std::unique_ptr<MCTargetAsmParser> TAP(TheTarget->createMCAsmParser(STI, *Parser, MCII, MCOptions));

  	if (!TAP) {
		printf("ERROR: createMCAsmParser() (does target support assembly parsing?)\n");
		goto cleanup;
	}

	Parser->setTargetParser(*TAP);

  	// AsmParser::Run in lib/MC/MCParser/AsmParser.cpp
  	/* first param is NoInitialTextSection
	   by supplying false -> YES initial text section and obviate ".text" in asm source */
  	rc = Parser->Run(false);
	printf("Parser->Run() returned %d\n", rc);

	cleanup:
  	return rc;
}

/* assemble using LLVM 

	this works by assembling twice:
		1) pass 1 outputs to temporary assembler file ("verbose asm"), so we 
		can parse the number of bytes each instruction is
		2) pass 2 outputs to temporary object file, so little relative
		addresses and such from branches will be resolved
*/
int 
llvm_svcs_assemble(
	/* in parameters */
	const char *srcText, 		/* eg: "mov r0, r0" */
	int dialect, 				/* eg: LLVM_SVCS_DIALECT_ATT */
	const char *triplet, 		/* eg: x86_64-thumb-linux-gnu */
	string abi,					/* usually "none", but "eabi" for mips */	
	int codeModel,				/* LLVM_SVCS_CM_JIT, LLVM_SVCS_CM_SMALL, etc. */
	int relocMode, 				/* LLVM_SVCS_DEFAULT, LLVM_SVCS_STATIC, etc. */

	/* callbacks */
	llvm_svcs_assemble_cb_type callback,

	/* out parameters */
	string &instrBytes,			/* output bytes */
	vector<int> &instrLengths,	/* instruction lengths */
	string &strErr				/* error string */
)
{
	int rc = -1;

	/* output for asm->obj */
	SmallString<1024> smallString;
    raw_svector_ostream rsvo(smallString);
	
	/* source code vars */
	std::string strSrc(srcText);
	std::unique_ptr<MemoryBuffer> mbSrc;
	SourceMgr *srcMgr;

	/* misc */
	MCContext *context;

	llvm::InitializeAllTargetInfos();
	llvm::InitializeAllTargetMCs();
	llvm::InitializeAllAsmParsers();
	llvm::InitializeAllDisassemblers();

	/*************************************************************************/
	/* the triple and the target */
	/*************************************************************************/

	// see /lib/Support/Triple.cpp for the details
	std::string machSpec(triplet);
	machSpec = Triple::normalize(machSpec);
	Triple TheTriple(machSpec);

	/* get the target specific parser
		if arch is blank, the triple is consulted */
	const Target *target = TargetRegistry::lookupTarget(/*arch*/"", TheTriple, strErr);
	if (!target) {
		strErr = "TargetRegistry::lookupTarget() failed\n" + strErr;
		return -1;
	}

	//printf("machine spec: %s\n", machSpec.c_str());
	//printf("Target.getName(): %s\n", target->getName());
	//printf("Target.getShortDescription(): %s\n", target->getShortDescription());

	/* from the target we get almost everything */
	std::unique_ptr<MCRegisterInfo> regInfo(target->createMCRegInfo(machSpec));
	std::unique_ptr<MCAsmInfo> asmInfo(target->createMCAsmInfo(*regInfo, machSpec));
	std::unique_ptr<MCInstrInfo> instrInfo(target->createMCInstrInfo()); /* describes target instruction set */
	MCSubtargetInfo *subTargetInfo = target->createMCSubtargetInfo(machSpec, "", ""); /* subtarget instr set */
	/* fixups, relaxations, objs, elfs */
	MCAsmBackend *asmBackend = target->createMCAsmBackend(*regInfo, machSpec, /* specific CPU */ "");
	MCInstPrinter *instrPrinter =  target->createMCInstPrinter(Triple(machSpec), /*variant*/0, *asmInfo, *instrInfo, *regInfo);

	/*************************************************************************/
	/* source code manager */
	/*************************************************************************/

	// llvm::SourceMgr (include/llvm/Support/SourceMgr.h) that holds assembler source
	// has vector of llvm::SrcBuffer encaps (Support/MemoryBuffer.h) and vector of include dirs
	srcMgr = new SourceMgr();
	mbSrc = MemoryBuffer::getMemBuffer(strSrc);
	srcMgr->AddNewSourceBuffer(std::move(mbSrc), SMLoc());
	if(callback) srcMgr->setDiagHandler(diag_cb, (void *)callback);

	/*************************************************************************/
	/* MC context, object file, code emitter, target options */
	/*************************************************************************/

	// MC/MCObjectFileInfo.h describes common object file formats
	MCObjectFileInfo objFileInfo;

	/* MC/MCContext.h */ 
	context = new MCContext(asmInfo.get(), regInfo.get(), &objFileInfo, srcMgr);

	/* yes, this is circular (MCContext requiring MCObjectFileInfo and visa
		versa, and is marked "FIXME" in llvm-mc.cpp */
	
	/* also see initMachOMCObjectFileInfo(), initELFMCObjectFileInfo(),
		initCOFFMCObjectFileInfo() ... will ask TT.getObjectFormat() if not
		specified */
	objFileInfo.InitMCObjectFileInfo(
		TheTriple, 
		map_reloc_mode(relocMode),
		map_code_model(codeModel),
		*context
	);

	/* code emitter llvm/MC/MCCodeEmitter.h
		has encodeInstruction() which maps MCInstr -> bytes 

		target returns with X86MCCodeEmitter, ARMMCCodeEmitter, etc.
	*/
	MCCodeEmitter *codeEmitter = target->createMCCodeEmitter(*instrInfo, *regInfo, *context);

	/* target opts */
	MCTargetOptions targetOpts;
	targetOpts.MCUseDwarfDirectory = false;
	targetOpts.ABIName = abi;

	/*************************************************************************/
	/* assemble to string */
	/*************************************************************************/

	/* the output stream ... a formatted_raw_ostream wraps a raw_ostream to provide
	   tracking of line and column position for padding and shit */
	/* but raw_ostream is abstract and is implemented by raw_fd_ostream, raw_string_ostream, etc. */
	std::string asmOut;
	raw_string_ostream rso(asmOut);
	formatted_raw_ostream fro(rso);
	std::unique_ptr<formatted_raw_ostream> pfro(&fro);

	// this is the assembler interface
	// -methods per .s statements (emit bytes, handle directive, etc.)
	// -remembers current section
	// -implementations that write a .s, or .o in various formats
	MCStreamer *streamer = target->createAsmStreamer(
		*context, /* the MC context */
		std::move(pfro), /* output stream (type: std::unique_ptr<formatted_raw_ostream> from Support/FormattedStream.h) */
		true, /* isVerboseAsm */
		false, /* useDwarfDirectory */
		instrPrinter, /* if given, the instruction printer to use (else, MCInstr representation is used) */
		codeEmitter, /* if given, a code emitter used to show instruction encoding inline with the asm */
		asmBackend,  /* the AsmBackend, (fixups, relaxation, objs and elfs) */
		true /* ShowInst (show encoding) */
	);

	rc = invoke_llvm_parsers(target, srcMgr, *context, *streamer, *asmInfo, 
		*subTargetInfo, *instrInfo, targetOpts, dialect);

	if(rc) {
		strErr = "invoking llvm parsers\n";
		goto cleanup;
	}

	/* flush the FRO (formatted raw ostream) */
	fro.flush();

	printf("got back:\n%s", asmOut.c_str());

	rc = asm_output_to_instr_counts(asmOut.c_str(), instrLengths);
	if(rc != 0) {
		strErr = "couldn't parse instruction lengths\n";
		goto cleanup;
	}

	for(auto i = instrLengths.begin(); i!=instrLengths.end(); ++i) {
		printf("%d\n", *i);
	}

	/*************************************************************************/
	/* assemble to object by creating a new streamer */
	/*************************************************************************/

	/* have tried various "reset" behaviors here, like codeEmitter->reset(),
		and re-initializing the source manager, but ultimately settled for
		a full context re-initialization */

	context = new MCContext(asmInfo.get(), regInfo.get(), &objFileInfo, srcMgr);
	objFileInfo.InitMCObjectFileInfo(TheTriple, map_reloc_mode(relocMode), map_code_model(codeModel), *context);
	codeEmitter = target->createMCCodeEmitter(*instrInfo, *regInfo, *context);

    streamer = target->createMCObjectStreamer(
		TheTriple,
        *context,
        *asmBackend,  /* (fixups, relaxation, objs and elfs) */
        rsvo, /* output stream raw_pwrite_stream */
        codeEmitter,
		*subTargetInfo,
		true, /* relax all fixups */
		true, /* incremental linker compatible */ 
        false /* DWARFMustBeAtTheEnd */
    );

	rc = invoke_llvm_parsers(target, srcMgr, *context, *streamer, *asmInfo, 
		*subTargetInfo, *instrInfo, targetOpts, dialect);

	if(rc) {
		strErr = "invoking llvm parsers\n";
		goto cleanup;
	}

	if(obj_output_to_bytes(smallString.data(), instrBytes)) {
		strErr = "parsing bytes from LLVM obj\n";
		goto cleanup;
	}
	
	/* dump to file for debugging */
	FILE *fp;
	fp = fopen("out.bin", "wb");
	fwrite(smallString.data(), 1, smallString.size(), fp);
	fclose(fp);

	dump_bytes((unsigned char *)(instrBytes.c_str()), instrBytes.size(), 0);

	rc = 0;
	cleanup:
	return rc;
}

void
llvm_svcs_triplet_decompose(const char *triplet, string &arch, string &vendor,
	string &os, string &environ, string &objFormat)
{
	//spec = llvm::sys::getDefaultTargetTriple();
	//std::string machSpec = "x86_64-apple-darwin14.5.0";
	//std::string machSpec = "x86_64-apple-darwin";
	//std::string machSpec = "x86_64-thumb-linux-gnu";
	//std::string machSpec = "x86_64-unknown-linux-gnu";
	Triple trip(triplet);

	arch = trip.getArchName();
	//string subarch = Triple::SubArchType(
	vendor = trip.getVendorName();
	os = trip.getOSName();
	environ = trip.getEnvironmentName();
	Triple::ObjectFormatType oft = trip.getObjectFormat();

	switch(oft) {
		case Triple::COFF: objFormat = "coff"; break;
		case Triple::ELF: objFormat = "elf"; break;
		case Triple::MachO: objFormat = "MachO"; break;
		case Triple::UnknownObjectFormat: objFormat = "unknown"; break;
	}
}
