#include "VRSystem.h"
#include <stdlib.h>
#include <iostream>
#include <boost/filesystem.hpp>
#include <execinfo.h>
#include <stdio.h>
#include <unistd.h>

void printBacktrace() {
    void *buffer[100];
    char **strings;

    int nptrs = backtrace(buffer, 100);
    printf("backtrace() returned %d addresses\n", nptrs);

    strings = backtrace_symbols(buffer, nptrs);
    if (strings != NULL) {
        for (int j = 0; j < nptrs; j++) printf("%s\n", strings[j]);
        free(strings);
    }
}

bool exists(string path) { return boost::filesystem::exists(path); }

bool makedir(string path) {
    if (path == "") return true;
    bool res = false;
    try { res = boost::filesystem::create_directory(path); }
    catch(...) { cout << "ERROR: makedir failed when trying to create directory '" + path + "'" << endl; }
    return res;
}

bool removeFile(string path) { return boost::filesystem::remove(path); }
string canonical(string path) { return boost::filesystem::canonical(path).string(); }

string getFileName(string path) {
    size_t sp = path.rfind('/');
    if (sp == string::npos) return path;
    return path.substr(sp+1, path.size());
}

string getFolderName(string path) {
    size_t sp = path.rfind('/');
    if (sp == string::npos) return "";
    return path.substr(0, sp);
}


int systemCall(string cmd) {
    return system(cmd.c_str());
}

bool compileCodeblocksProject(string path) {
    if (!exists(path)) { cout << "compiling codeblocks project '" << path << "' failed, path not found!"; return false; }
    string cmd = "codeblocks --no-ipc --no-splash-screen --target='Release' --build ";
    cmd += path;
    cout << "compile codeblocks project: " << cmd << endl;
    return systemCall(cmd) == 0;
}
