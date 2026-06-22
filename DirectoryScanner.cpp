#include "DirectoryScanner.h"
#include <dirent.h>
#include <vector>

std::vector<std::string> DirectoryScanner::scan(const std::string& dir){
    std::vector<std::string> files;
    DIR* dp=opendir(dir.c_str());
    if(dp==nullptr){
        return files;
    }
    struct dirent* entry;
    while((entry=readdir(dp))!=nullptr){
        std::string name=entry->d_name;
        if(name == "." || name == ".."){
            continue;
        }
        if(entry->d_type==DT_REG || entry->d_type==DT_UNKNOWN){
            files.push_back(name);
        }
    }
    closedir(dp);
    return files;
}
