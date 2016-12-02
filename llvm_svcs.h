/* abstract away LLVM details from the rest of LLL */

#define LLVM_SVCS_CB_NOTE 0
#define LLVM_SVCS_CB_WARNING 1
#define LLVM_SVCS_CB_ERROR 2

#define LLVM_SVCS_DIALECT_UNSPEC 0
#define LLVM_SVCS_DIALECT_ATT 1
#define LLVM_SVCS_DIALECT_INTEL 2

#define LLVM_SVCS_CM_DEFAULT 0
#define LLVM_SVCS_CM_SMALL 1
#define LLVM_SVCS_CM_KERNEL 2
#define LLVM_SVCS_CM_MEDIUM 3
#define LLVM_SVCS_CM_LARGE 4

#define LLVM_SVCS_RM_DEFAULT 0
#define LLVM_SVCS_RM_STATIC 1
#define LLVM_SVCS_RM_PIC 2
#define LLVM_SVCS_RM_DYNAMIC_NO_PIC 3

/* misc */
void llvm_svcs_init(void);

void llvm_svcs_triplet_decompose(const char *triplet, string &arch,
	string &subArch, string &vendor, string &os, string &environ, 
	string &objFormat);

/* assemble */
typedef void (*llvm_svcs_assemble_cb_type)(int type, const char *fileName, 
	int lineNum, const char *message);

int llvm_svcs_assemble(const char *src, int dialect, const char *triplet, 
	string abi, int codeModel, int relocMode, 
	llvm_svcs_assemble_cb_type callback, string &outBytes, 
	string &err);

/* disassemble */
int llvm_svcs_disasm_single(const char *triplet, uint8_t *src,
	int src_len, uint64_t addr, string &result, int &instrLen);

int llvm_svcs_disasm_lengths(const char *triplet, uint8_t *src, 
	int src_len, uint64_t addr, vector<int> &lengths);

