#pragma once

#include <string>

#include "Definitions.h"
#include "ltl/Specification.h"

#include "owl.h"

namespace ltl {

class Parser {
private:
    const owl::OwlThread& owl;

protected:
    Parser(const owl::OwlThread& owl);

    virtual Specification parse_string_with_factory(
            owl::FormulaFactory& factory,
            const std::string& text
    ) const = 0;

public:
    virtual ~Parser();

    std::string parse_file(const std::string& input_file);
    Specification parse_string(const std::string& input_string, const int verbosity = 0) const;
};

}
