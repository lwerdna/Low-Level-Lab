/* abstract away LLVM details from the rest of LLL */

#define LLVM_SVCS_CB_NOTE 0
#define LLVM_SVCS_CB_WARNING 1
#define LLVM_SVCS_CB_ERROR 2
        
#define LLVM_SVCS_DIALECT_UNSPEC 0
#define LLVM_SVCS_DIALECT_ATT 1
#define LLVM_SVCS_DIALECT_INTEL 2

typedef void (*llvm_svcs_assemble_cb_type)(int type, const char *fileName, 
    int lineNum, const char *message);

int llvm_svcs_assemble(const char *src, int dialect, const char *triplet, 
    string &outText, string &outBytes, string &err, 
    llvm_svcs_assemble_cb_type callback);

void llvm_svcs_triplet_decompose(const char *triplet, string &arch,
    string &vendor, string &os, string &environ, string &objFormat);
