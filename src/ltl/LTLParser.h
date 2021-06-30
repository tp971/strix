#pragma once

#include "Parser.h"

namespace ltl {

class LTLParser : public Parser {
private:
    const std::vector<std::string>& inputs;
    const std::vector<std::string>& outputs;
    const std::string finite;

protected:
    Specification parse_string_with_factory(
            owl::FormulaFactory& factory,
            const std::string& text
    ) const;
public:
    LTLParser(const owl::OwlThread& owl, const std::vector<std::string>& inputs, const std::vector<std::string>& outputs, const std::string finite = "");
    ~LTLParser();
};

}
