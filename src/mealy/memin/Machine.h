#pragma once

#include "Definitions.h"
#include "util/SpecSeq.h"

namespace mealy {

//array index
template <typename I>
inline I ai(I x, I y, I ySize) {
    return x*ySize + y;
}

typedef uint32_t state_id_t;
constexpr state_id_t TOP_STATE = std::numeric_limits<state_id_t>::max();
constexpr state_id_t NONE_STATE = std::numeric_limits<state_id_t>::max() - 1;

struct Transition {
    state_id_t nextState;
    const SpecSeq<letter_t> input;
    const std::vector<SpecSeq<letter_t>> output;

    Transition(
            const state_id_t nextState,
            const SpecSeq<letter_t> input,
            const std::vector<SpecSeq<letter_t>> output
    ) : nextState(nextState), input(input), output(std::move(output)) {}
};

typedef std::vector<std::vector<Transition>> machine_t;

}
