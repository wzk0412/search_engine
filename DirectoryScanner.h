#pragma once
#include <vector>
#include <string>

class DirectoryScanner
{
public:
    static std::vector<std::string>scan(const std::string&dir);
private:
    DirectoryScanner()=delete;

};

