#include <iostream>
#include <string>
#include <regex>

int main(int argc,char **argv)
{
    std::string s("abc123def");
    std::regex  r("([0-9]+)");
    std::smatch m;
    if (std::regex_search(s, m, r)){
        std::cout << "Matches '"<<m.str() << "'\n";
    }
    return(0);
}
