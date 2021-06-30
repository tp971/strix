#pragma once

#include <iostream>
#include <limits>
#include <tuple>

#include <boost/functional/hash.hpp>

#include "owl.h"
#include "Definitions.h"
#include "util/SpecSeq.h"
#include "util/Quine.h"
#include "memin/MeMin.h"

namespace mealy {

enum class Semantic { MEALY, MOORE };

struct Successor {
    state_id_t successor;
    std::vector<SpecSeq<letter_t>> output;

    Successor() : successor(0), output(0) {}
    Successor(const state_id_t successor, const std::vector<SpecSeq<letter_t>> output) :
        successor(successor), output(std::move(output)) {}

    bool operator==(const Successor &other) const {
        return (successor == other.successor && output == other.output);
    }
    bool operator<(const Successor& other) const {
        return std::tie(successor, output) < std::tie(other.successor, other.output);
    }
};

class MealyMachine {
public:
    const std::vector<std::string> inputs;
    const std::vector<std::string> outputs;
    const size_t n_inputs;
    const size_t n_outputs;
    const Semantic semantic;

private:
    machine_t machine;
    machine_t min_machine;

    std::vector<SpecSeq<node_id_t>> state_labels;
    std::vector<SpecSeq<node_id_t>> min_state_labels;
    std::vector<int> state_label_accumulated_bits;
    int state_label_bits;

    //std::vector<state_id_t> min_map;

public:
    MealyMachine(const std::vector<std::string> inputs_, const std::vector<std::string> outputs_, const Semantic semantic) :
        inputs(std::move(inputs_)),
        outputs(std::move(outputs_)),
        n_inputs(inputs.size()),
        n_outputs(outputs.size()),
        semantic(semantic),
        state_label_bits(-1)
    { }
    ~MealyMachine() { }

    void setMachine(machine_t new_machine);
    void setStateLabels(std::vector<SpecSeq<node_id_t>> labels, const int bits, std::vector<int> accumulated_bits);
    SpecSeq<node_id_t> getStateLabel(const state_id_t s, const bool use_minimized = false) const;

    void minimizeMachine(const int verbosity = 0);
    state_id_t numberOfStates() const;
    state_id_t numberOfMinStates() const;
    int getStateLabelBits() const;
    const std::vector<std::vector<Transition>>& getTransitions(const bool minimize) const;
    bool hasLabels() const;
    bool hasMinimized() const;

    void print_kiss(std::ostream& out) const;
    void print_dot(std::ostream& out) const;
};

}
