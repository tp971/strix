#include "Parser.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace ltl {

Parser::Parser(const owl::OwlThread& owl) : owl(owl) { }

Parser::~Parser() { }

std::string Parser::parse_file(const std::string& input_file) {
    // read input file
    std::string input_string;

    std::ifstream infile(input_file);
    if (infile.is_open()) {
        input_string = std::string((std::istreambuf_iterator<char>(infile)), std::istreambuf_iterator<char>());
    }
    else {
        throw std::runtime_error("could not open input file");
    }
    if (infile.bad()) {
        throw std::runtime_error("could not read input file");
    }
    infile.close();

    return input_string;
}

Specification Parser::parse_string(const std::string& input_string, const int verbosity) const {
    if (verbosity >= 4) {
        std::cout << "Input: " << std::endl << input_string << std::endl;
    }
    owl::FormulaFactory factory = owl.createFormulaFactory();
    Specification spec = parse_string_with_factory(factory, input_string);
    if (verbosity >= 2) {
        std::cout << "Formula: ";
        spec.formula.print();
        std::cout << "Inputs:";
        for (const auto& i : spec.inputs) {
            std::cout << " " << i;
        }
        std::cout << std::endl;
        std::cout << "Outputs:";
        for (const auto& o : spec.outputs) {
            std::cout << " " << o;
        }
        std::cout << std::endl;
    }

    return spec;
}

}
