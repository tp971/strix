#include "MealyMachine.h"

#include <iomanip>
#include <bitset>
#include <cmath>
#include <map>
#include <algorithm>

namespace mealy {

void MealyMachine::setStateLabels(std::vector<SpecSeq<node_id_t>> labels, int bits, std::vector<int> accumulated_bits) {
    state_label_bits = bits;
    state_labels = std::move(labels);
    state_label_accumulated_bits = std::move(accumulated_bits);
}

SpecSeq<node_id_t> MealyMachine::getStateLabel(const state_id_t s, const bool use_minimized) const {
    if (use_minimized) {
        return min_state_labels[s];
    }
    else {
        return state_labels[s];
    }
}

void MealyMachine::setMachine(machine_t new_machine) {
    machine = std::move(new_machine);
}

void MealyMachine::minimizeMachine(const int verbosity) {
    std::vector<std::vector<state_id_t> > newStates;

    int result = 0;
    try {
        result = minimizeMachineMeMin(machine, 0, n_inputs, n_outputs, min_machine, newStates, verbosity);
    }
    catch (const std::bad_alloc& ex) {
        if (verbosity >= 1) {
            std::cerr << "Error: Minimization of Mealy machine failed due to insufficient memory." << std::endl;
        }
        min_machine.clear();
    }

    if (result == 1) {
        // check if labels need to be re-assigned
        if (hasLabels()) {
            // assign to each class a label from a state mapped to that class, if possible
            // use greedy approach, simply take the next free label from that class
            // if greedy approach fails, assign labels sequentially as a fallback

            bool label_found = true;
            std::vector<bool> stateLabelAssigned(machine.size(), false);
            for (state_id_t sClass = 0; sClass < newStates.size(); sClass++) {
                std::vector<state_id_t>& statesInClass = newStates[sClass];
                std::sort(statesInClass.begin(), statesInClass.end());
                label_found = false;
                for (const state_id_t oldState : statesInClass) {
                    if (!stateLabelAssigned[oldState]) {
                        stateLabelAssigned[oldState] = true;
                        label_found = true;
                        min_state_labels.push_back(state_labels[oldState]);
                        break;
                    }
                }
                if (!label_found) {
                    break;
                }
            }
            label_found = false;
            if (!label_found) {
                // fall back to sequential assignment
                min_state_labels = state_labels;
            }
        }
    }
    else {
        // no minimized machine
        min_machine.clear();
    }
}

state_id_t MealyMachine::numberOfStates() const {
    return machine.size();
}
state_id_t MealyMachine::numberOfMinStates() const {
    return min_machine.size();
}
int MealyMachine::getStateLabelBits() const {
    return state_label_bits;
}
bool MealyMachine::hasLabels() const {
    return state_label_bits >= 0;
}
bool MealyMachine::hasMinimized() const {
    return min_machine.size() > 0;
}

const std::vector<std::vector<Transition>>& MealyMachine::getTransitions(const bool minimize) const {
    if (minimize) {
        return min_machine;
    }
    else {
        return machine;
    }
}

void MealyMachine::print_kiss(std::ostream& out) const {
    const bool use_labels = hasLabels();
    const bool minimize = hasMinimized();

    out << ".inputs";
    for (const auto& input : inputs) {
        out << " " << input;
    }
    out << std::endl;
    out << ".outputs";
    for (const auto& output : outputs) {
        out << " " << output;
    }

    const std::vector<std::vector<Transition>>& kiss_machine = getTransitions(minimize);

    out << std::endl;
    out << ".i " << n_inputs << std::endl;
    out << ".o " << n_outputs << std::endl;
    size_t n_transitions = 0;
    for (state_id_t s = 0; s < kiss_machine.size(); s++) {
        n_transitions += kiss_machine[s].size();
    }
    out << ".p " << n_transitions << std::endl;
    out << ".s " << kiss_machine.size() << std::endl;
    if (use_labels) {
        out << ".r " << getStateLabel(0, minimize).toVectorString(state_label_accumulated_bits) << std::endl;
    }
    else {
        out << ".r S0" << std::endl;
    }
    for (state_id_t s = 0; s < kiss_machine.size(); s++) {
        for (const auto& t : kiss_machine[s]) {
            out << t.input.toString(n_inputs);
            out << " ";
            if (use_labels) {
                out << getStateLabel(s, minimize).toVectorString(state_label_accumulated_bits);
            }
            else {
                out << "S" << s;
            }
            out << " ";
            if (use_labels) {
                out << getStateLabel(t.nextState, minimize).toVectorString(state_label_accumulated_bits);
            }
            else {
                out << "S" << t.nextState;
            }
            out << " ";
            bool first = true;
            for (const auto& output : t.output) {
                if (!first) {
                    out << " + ";
                }
                first = false;
                out << output.toString(n_outputs);
            }
            out << std::endl;
        }
    }
}

void MealyMachine::print_dot(std::ostream& out) const {
    const bool use_labels = hasLabels();
    const bool minimize = hasMinimized();

    const std::vector<std::vector<Transition>>& kiss_machine = getTransitions(minimize);

    out << "digraph \"\" {" << std::endl;
    out << "graph [rankdir=LR,ranksep=0.8,nodesep=0.2];" << std::endl;
    out << "node [shape=circle];" << std::endl;
    out << "edge [fontname=mono];" << std::endl;
    out << "init [shape=point,style=invis];" << std::endl;

    for (state_id_t s = 0; s < kiss_machine.size(); s++) {
        out << s << " [label=\"" << s << "\"";
        if (use_labels) {
            out << ",tooltip=\"" << getStateLabel(s, minimize).toStateLabelledString(state_label_accumulated_bits) << "\"";
        }
        out << "];" << std::endl;
    }

    out << "init -> 0 [penwidth=0,tooltip=\"initial state\"];" << std::endl;

    for (state_id_t s = 0; s < kiss_machine.size(); s++) {
        // first collect transitions to the same state
        std::map<state_id_t, std::vector<std::pair<SpecSeq<letter_t>, std::vector<SpecSeq<letter_t>> > > > stateTransitions;
        for (const auto& t : kiss_machine[s]) {
            stateTransitions[t.nextState].push_back({ t.input, t.output });
        }

        for (const auto& t : stateTransitions) {
            out << s << " -> " << t.first << " [label=\"";
            for (const auto& l : t.second) {
                out << l.first.toString(n_inputs) << "/";
                bool first = true;
                for (const auto& output : l.second) {
                    if (!first) {
                        out << "+";
                    }
                    first = false;
                    out << output.toString(n_outputs);
                }
                out << "\\l";
            }
            out << "\",labeltooltip=\"";
            for (const auto& l : t.second) {
                out << l.first.toLabelledString(n_inputs, inputs) << "/";
                bool first = true;
                for (const auto& output : l.second) {
                    if (!first) {
                        out << "&#8744;";
                    }
                    first = false;
                    out << output.toLabelledString(n_outputs, outputs);
                }
                out << "&#10;";
            }
            out << "\"];" << std::endl;
        }
    }
    out << "}" << std::endl;
}

}
