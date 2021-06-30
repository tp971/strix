#include "LTLParser.h"

#include <iostream>
#include <sstream>
#include <algorithm>

namespace ltl {

LTLParser::LTLParser(const owl::OwlThread& owl, const std::vector<std::string>& inputs, const std::vector<std::string>& outputs, const std::string finite) :
    Parser(owl),
    inputs(inputs),
    outputs(outputs),
    finite(finite)
{ }

LTLParser::~LTLParser() { }

Specification LTLParser::parse_string_with_factory(
        owl::FormulaFactory& factory,
        const std::string& text
) const {
    // Create list of variables for parser
    std::vector<std::string> variables;
    variables.insert(variables.end(), inputs.cbegin(), inputs.cend());
    variables.insert(variables.end(), outputs.cbegin(), outputs.cend());

    // Parse with provided variables
    const bool use_finite = !finite.empty();
    owl::Formula formula =
        use_finite ? factory.parseFinite(text, variables) : factory.parse(text, variables);

    // Create list of variables for specification
    std::vector<std::string> spec_inputs = inputs;
    std::vector<std::string> spec_outputs = outputs;
    if (use_finite) {
        if (std::find(variables.begin(), variables.end(), finite) != variables.end()) {
            std::stringstream message;
            message << "Proposition '" << finite << "' for LTLf transformation already appears in list of propositions. ";
            message << "Please rename it manually by giving a different argument to the '--from-ltlf' option.";
            throw std::invalid_argument(message.str());
        }
        spec_outputs.emplace_back(finite);
    }

    // Return specification with input and output variables
    return Specification(std::move(formula), std::move(spec_inputs), std::move(spec_outputs));
}

}
