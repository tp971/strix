#include "ParityAutomatonTree.h"

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cmath>

namespace aut {

ParityAutomatonTreeNode::ParityAutomatonTreeNode(
        const owl::Tag tag, const NodeType node_type, const Parity parity_type, const color_t max_color,
        const node_id_t round_robin_size, const bool parity_child,
        const color_t dp, std::vector<std::unique_ptr<ParityAutomatonTree>> _children
) :
    ParityAutomatonTree(node_type, parity_type, max_color),
    tag(tag),
    children(std::move(_children)),
    parity_child(parity_child),
    dp(dp),
    round_robin_size(round_robin_size)
{ }

ParityAutomatonTreeNode::~ParityAutomatonTreeNode() { }

void ParityAutomatonTreeNode::getInitialState(product_state_t& state) {
    // save index in product state
    state_index = state.size();
    if (round_robin_size > 1) {
        // round-robin counter
        state.push_back(0);
    }
    if (round_robin_size > 0 && parity_child) {
        // inverse minimal parity seen
        state.push_back(0);
    }
    for (auto &child : children) {
        child->getInitialState(state);
    }
}

ColorScore ParityAutomatonTreeNode::getSuccessor(const product_state_t& state, product_state_t& new_state, letter_t letter) {
    if (isBottomState(state)) {
        setBottomState(new_state);
        return ColorScore(1 - parity_type, 0.0, 1.0);
    }
    else if (isTopState(state)) {
        setTopState(new_state);
        return ColorScore(parity_type, 1.0, 1.0);
    }

    size_t round_robin_index = state_index;
    size_t min_parity_index = state_index;
    node_id_t round_robin_counter = 0;
    if (round_robin_size > 1) {
        round_robin_counter = state[round_robin_index];
        min_parity_index += 1;
    }
    color_t min_parity = dp;
    if (round_robin_size > 0 && parity_child) {
        min_parity -= state[min_parity_index];
    }

    node_id_t buchi_index = 0;
    size_t active_children = 0;

    color_t max_weak_color = 0;
    color_t min_weak_color = 1;
    color_t min_buchi_color = 1;

    double score = 0.0;
    double weights = 0.0;

    for (auto &child : children) {
        const ColorScore cs = child->getSuccessor(state, new_state, letter);
        const color_t child_color = cs.color;
        double child_score = cs.score;
        double child_weight = cs.weight;

        if (child->isBottomState(new_state)) {
            if (tag == owl::CONJUNCTION) {
                setBottomState(new_state);
                return ColorScore(1 - parity_type, 0.0, 1.0);
            }
        }
        else if (child->isTopState(new_state)) {
            if (tag == owl::DISJUNCTION) {
                setTopState(new_state);
                return ColorScore(parity_type, 1.0, 1.0);
            }
        }
        else {
            active_children++;

            constexpr double log_one_half = log(0.5);
            if (tag == owl::CONJUNCTION) {
                child_weight *= log(child_score);
            }
            else {
                child_weight *= log(1.0 - child_score);
            }
            child_weight /= log_one_half;
        }

        bool increase_score = false;
        bool decrease_score = false;
        switch (child->node_type) {
            case NodeType::WEAK:
                max_weak_color = std::max(max_weak_color, child_color);
                min_weak_color = std::min(min_weak_color, child_color);
                break;
            case NodeType::BUCHI:
            case NodeType::CO_BUCHI:
                if (
                        (tag == owl::CONJUNCTION && child->node_type == NodeType::BUCHI) ||
                        (tag == owl::DISJUNCTION && child->node_type == NodeType::CO_BUCHI)
                ) {
                    if (child_color == 0 && round_robin_counter == buchi_index) {
                        if (child->node_type == NodeType::BUCHI) {
                            increase_score = true;
                        }
                        else if (child->node_type == NodeType::CO_BUCHI) {
                            decrease_score = true;
                        }
                        round_robin_counter++;
                    }
                    buchi_index++;
                }
                else {
                   min_buchi_color = std::min(min_buchi_color, child_color);
                }
                break;
            case NodeType::PARITY:
                if (parity_type == child->parity_type) {
                    if (child_color < min_parity) {
                        min_parity = child_color;
                        if (min_parity % 2 == parity_type) {
                            increase_score = true;
                        }
                        else {
                            decrease_score = true;
                        }
                    }
                }
                else {
                    if (child_color + 1 < min_parity) {
                        min_parity = child_color + 1;
                        if (min_parity % 2 == parity_type) {
                            increase_score = true;
                        }
                        else {
                            decrease_score = true;
                        }
                    }
                }
                break;
        }

        if (increase_score) {
            child_score = (0.75 + 0.25*child_score);
            child_weight *= 2.0;
        }
        else if (decrease_score) {
            child_score = (0.25*child_score);
            child_weight *= 2.0;
        }
        score += child_score * child_weight;
        weights += child_weight;
    }

    if (active_children == 0) {
        // discard children
        if (tag == owl::CONJUNCTION) {
            setTopState(new_state);
            return ColorScore(parity_type, 1.0, 1.0);
        }
        else {
            setBottomState(new_state);
            return ColorScore(1 - parity_type, 0.0, 1.0);
        }
    }

    score /= weights;

    color_t color;
    bool reset = false;

    if (tag == owl::CONJUNCTION && max_weak_color != 0) {
        reset = true;
        color = 1 - parity_type;
    }
    else if (tag == owl::DISJUNCTION && min_weak_color == 0) {
        reset = true;
        color = parity_type;
    }
    else if (min_buchi_color == 0) {
        reset = true;
        color = 0;
    }
    else if (round_robin_counter == round_robin_size) {
        reset = true;
        if (parity_child) {
            color = min_parity;
        }
        else {
            if (tag == owl::CONJUNCTION) {
                color = parity_type;
            }
            else {
                color = 1 - parity_type;
            }
        }
    }
    else {
        // output neutral color, not accepting for conjunction and accepting for disjunction
        color = max_color;
    }

    if (reset) {
        round_robin_counter = 0;
        min_parity = dp;
    }
    if (round_robin_size > 1) {
        new_state[round_robin_index] = round_robin_counter;
    }
    if (round_robin_size > 0 && parity_child) {
        new_state[min_parity_index] = dp - min_parity;
    }
    return ColorScore(color, score, weights);
}

void ParityAutomatonTreeNode::setState(product_state_t& new_state, node_id_t state) {
    size_t round_robin_index = state_index;
    size_t min_parity_index = state_index;
    if (round_robin_size > 1) {
        new_state[round_robin_index] = state;
        min_parity_index += 1;
    }
    if (round_robin_size > 0 && parity_child) {
        new_state[min_parity_index] = state;
    }
    for (auto &child : children) {
        child->setState(new_state, state);
    }
}

void ParityAutomatonTreeNode::setTopState(product_state_t& new_state) {
    if (tag == owl::DISJUNCTION) {
        setState(new_state, NODE_NONE_TOP);
        new_state[state_index] = NODE_TOP;
    }
    else {
        size_t round_robin_index = state_index;
        size_t min_parity_index = state_index;
        if (round_robin_size > 1) {
            new_state[round_robin_index] = NODE_NONE;
            min_parity_index += 1;
        }
        if (round_robin_size > 0 && parity_child) {
            new_state[min_parity_index] = NODE_NONE;
        }
        for (auto &child : children) {
            child->setTopState(new_state);
        }
    }
}

void ParityAutomatonTreeNode::setBottomState(product_state_t& new_state) {
    if (tag == owl::CONJUNCTION) {
        setState(new_state, NODE_NONE_BOTTOM);
        new_state[state_index] = NODE_BOTTOM;
    }
    else {
        size_t round_robin_index = state_index;
        size_t min_parity_index = state_index;
        if (round_robin_size > 1) {
            new_state[round_robin_index] = NODE_NONE;
            min_parity_index += 1;
        }
        if (round_robin_size > 0 && parity_child) {
            new_state[min_parity_index] = NODE_NONE;
        }
        for (auto &child : children) {
            child->setBottomState(new_state);
        }
    }
}

bool ParityAutomatonTreeNode::isTopState(const product_state_t& state) const {
    if (tag == owl::DISJUNCTION) {
        // need to check both in case of nested disjunction/conjunction
        return state[state_index] == NODE_TOP && state[state_index + 1] == NODE_NONE_TOP;
    }
    else {
        for (auto &child : children) {
            if (!child->isTopState(state)) {
                return false;
            }
        }
        return true;
    }
}
bool ParityAutomatonTreeNode::isBottomState(const product_state_t& state) const {
    if (tag == owl::CONJUNCTION) {
        // need to check both in case of nested conjunction/disjunction
        return state[state_index] == NODE_BOTTOM && state[state_index + 1] == NODE_NONE_BOTTOM;
    }
    else {
        for (auto &child : children) {
            if (!child->isBottomState(state)) {
                return false;
            }
        }
        return true;
    }
}

int ParityAutomatonTreeNode::getMinIndex() const {
    int min_index = std::numeric_limits<int>::max();
    for (const auto& child : children) {
        min_index = std::min(min_index, child->getMinIndex());
    }
    return min_index;
}

letter_t ParityAutomatonTreeNode::getMaximumAlphabetSize() const {
    letter_t max_size = MIN_LETTER;
    for (const auto& child : children) {
        max_size = std::max(max_size, child->getMaximumAlphabetSize());
    }
    return max_size;
}

std::set<letter_t> ParityAutomatonTreeNode::getAlphabet() const {
    std::set<letter_t> alphabet;
    for (const auto& child : children) {
        std::set<letter_t> child_alphabet = child->getAlphabet();
        alphabet.insert(child_alphabet.begin(), child_alphabet.end());
    }
    return alphabet;
}

void ParityAutomatonTreeNode::print(const int verbosity, const int indent) const {
    std::cout << std::setfill(' ') << std::setw(2*indent) << "";
    std::cout << "*) ";
    switch(tag) {
        case owl::CONJUNCTION:
            std::cout << "Conjunction";
            break;
        case owl::DISJUNCTION:
            std::cout << "Disjunction";
            break;
        case owl::BICONDITIONAL:
            std::cout << "Biconditional";
            break;
    }
    std::cout << " (";
    print_type();
    std::cout << ")" << std::endl;

    size_t round_robin_index = state_index;
    size_t min_parity_index = state_index;
    if (round_robin_size > 1 && tag != owl::BICONDITIONAL) {
        std::cout << std::setfill(' ') << std::setw(2*(indent+1)) << "";
        std::cout << round_robin_index << ") Round Robin LAR" << std::endl;;
        min_parity_index += 1;
    }
    if (round_robin_size > 0) {
        for (unsigned int i = 0;
            (tag == owl::BICONDITIONAL && i < round_robin_size) ||
            (tag != owl::BICONDITIONAL && i < 1 && parity_child);
        i++) {
            std::cout << std::setfill(' ') << std::setw(2*(indent+1)) << "";
            std::cout << min_parity_index + i << ") Parity LAR" << std::endl;;
        }
    }

    for (const auto& child : children) {
        child->print(verbosity, indent + 1);
    }
}

}
