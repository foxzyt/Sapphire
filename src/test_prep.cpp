#include <iostream>
#include <fstream>
#include "preprocessor/preprocessor.h"

int main() {
    std::ifstream file("tests\\UI\\calculator.sp");
    std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    Preprocessor p;
    std::string out = p.process(source);
    
    std::ofstream out_file("prep_out.sp");
    out_file << out;
    return 0;
}
