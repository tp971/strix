#include "ParityAutomatonTree.h"

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <bitset>

namespace aut {

ParityAutomatonTreeLeaf::ParityAutomatonTreeLeaf(
        Automaton& _automaton,
        const owl::Reference _reference
) :
    ParityAutomatonTree(_automaton.getNodeType(), _automaton.getParityType(), _automaton.getMaxColor()),
    automaton(_automaton),
    reference(std::move(_reference))
{
    automaton.setAlphabetSize(reference.alphabet_mapping.size());
}

ParityAutomatonTreeLeaf::~ParityAutomatonTreeLeaf() { }

void ParityAutomatonTreeLeaf::getInitialState(product_state_t& state) {
    // save index in product state
    state_index = state.size();
    // initial state of automaton
    state.push_back(0);
}

ColorScore ParityAutomatonTreeLeaf::getSuccessor(const product_state_t& state, product_state_t& new_state, letter_t letter) {
    if (isBottomState(state)) {
        setBottomState(new_state);
        return ColorScore(1 - parity_type, 0.0, 1.0);
    }
    else if (isTopState(state)) {
        setTopState(new_state);
        return ColorScore(parity_type, 1.0, 1.0);
    }

    const node_id_t local_state = state[state_index];

    letter_t new_letter = 0;

    for (const auto& map : reference.alphabet_mapping) {
        new_letter |= ((letter & ((letter_t)1 << map.first)) >> map.first) << map.second;
    }

    ScoredEdge edge = automaton.getSuccessor(local_state, new_letter);
    new_state[state_index] = edge.successor;

    return edge.cs;
}

void ParityAutomatonTreeLeaf::setState(product_state_t& new_state, node_id_t state) {
    new_state[state_index] = state;
}

void ParityAutomatonTreeLeaf::setTopState(product_state_t& new_state) {
    new_state[state_index] = NODE_TOP;
}

void ParityAutomatonTreeLeaf::setBottomState(product_state_t& new_state) {
    new_state[state_index] = NODE_BOTTOM;
}

bool ParityAutomatonTreeLeaf::isTopState(const product_state_t& state) const {
    return state[state_index] == NODE_TOP;
}
bool ParityAutomatonTreeLeaf::isBottomState(const product_state_t& state) const {
    return state[state_index] == NODE_BOTTOM;
}

int ParityAutomatonTreeLeaf::getMinIndex() const {
    return reference.index;
}

letter_t ParityAutomatonTreeLeaf::getMaximumAlphabetSize() const {
    return reference.alphabet_mapping.size();
}

std::set<letter_t> ParityAutomatonTreeLeaf::getAlphabet() const {
    std::set<letter_t> alphabet;
    std::transform(reference.alphabet_mapping.begin(), reference.alphabet_mapping.end(),
            std::inserter(alphabet, alphabet.end()), [](const auto& map) -> letter_t { return map.first; });
    return alphabet;
}

void ParityAutomatonTreeLeaf::print(const int verbosity, const int indent) const {
    std::cout << std::setfill(' ') << std::setw(2*indent) << "";

    std::cout << state_index << ")";
    std::cout << " A[" << reference.index << "]";
    std::cout << " (";
    automaton.print_type();
    std::cout << ")";
    if (verbosity >= 2) {
        std::cout << " ";
        reference.formula.print();
    }
    else {
        std::cout << std::endl;
    }
    if (verbosity >= 4) {
        for (const auto& map : reference.alphabet_mapping) {
            std::cout << std::setfill(' ') << std::setw(2*(indent+2)) << "" <<
                         "p" << map.first << " -> " << map.second << std::endl;
        }
    }
}

}
