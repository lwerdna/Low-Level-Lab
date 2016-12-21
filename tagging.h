
int
tagging_python_init(char *progName);

int tagging_load_results(const char *fpath, IntervalMgr &mgr);

int tagging_get_py_ver(string &ver);

int 
tagging_modules_find_dir(string path, vector<string> &results);

int tagging_modules_find_all(vector<string> &taggers);

int tagging_module_test(string pathModule, string pathTarget, bool &result);

int tagging_modules_test(vector<string> modPaths, string pathTarget,
	vector<string> &result);

int tagging_module_exec(string pathModule, string pathTarget, string pathTags);

