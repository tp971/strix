#pragma once

#include <string>

#include "Definitions.h"

#include "owl.h"

namespace ltl {

class Specification {
public:
    owl::Formula formula;
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;

    Specification(
        owl::Formula formula,
        std::vector<std::string> inputs,
        std::vector<std::string> outputs
    ) :
        formula(formula),
        inputs(inputs),
        outputs(outputs)
    {}
};

}
