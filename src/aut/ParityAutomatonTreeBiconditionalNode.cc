#include "ParityAutomatonTree.h"

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <bitset>

namespace aut {

ParityAutomatonTreeBiconditionalNode::ParityAutomatonTreeBiconditionalNode(
        const NodeType node_type, const Parity parity_type, const color_t max_color,
        const node_id_t round_robin_size, const bool parity_child,
        const color_t dp, const int parity_child_index,
        std::vector<std::unique_ptr<ParityAutomatonTree>> _children
) :
    ParityAutomatonTreeNode(owl::BICONDITIONAL, node_type, parity_type, max_color, round_robin_size, parity_child, dp, std::move(_children)),
    parity_child_index(parity_child_index),
    d1(children[1 - parity_child_index]->max_color),
    d2(children[parity_child_index]->max_color)
{ }

ParityAutomatonTreeBiconditionalNode::~ParityAutomatonTreeBiconditionalNode() { }

void ParityAutomatonTreeBiconditionalNode::getInitialState(product_state_t& state) {
    // save index in product state
    state_index = state.size();
    for (unsigned int i = 0; i < round_robin_size; i++) {
        // inverse minimal parity seen
        state.push_back(0);
    }
    for (auto &child : children) {
        child->getInitialState(state);
    }
}

ColorScore ParityAutomatonTreeBiconditionalNode::getSuccessor(const product_state_t& state, product_state_t& new_state, letter_t letter) {
    if (isBottomState(state)) {
        setBottomState(new_state);
        return ColorScore(1 - parity_type, 0.0, 1.0);
    }
    else if (isTopState(state)) {
        setTopState(new_state);
        return ColorScore(parity_type, 1.0, 1.0);
    }

    color_t min_parity = dp;
    for (unsigned int i = 0; i < round_robin_size; i++) {
        min_parity = std::min(min_parity, dp - state[state_index + i]);
    }

    size_t active_children = 0;

    bool bottom = false;
    bool top = false;
    color_t child_colors[2];

    double min_score = 1.0;
    double max_score = 0.0;

    double score = 0.0;
    double weights = 0.0;

    int child_index = 0;
    for (auto &child : children) {
        const ColorScore cs = child->getSuccessor(state, new_state, letter);
        const color_t child_color = cs.color;

        double child_score = cs.score;
        double child_weight = cs.weight;
        bool increase_score = false;
        bool decrease_score = false;

        min_score = std::min(min_score, child_score);
        max_score = std::max(max_score, child_score);

        child_colors[child_index] = child_color;

        if (child->isBottomState(new_state)) {
            bottom = true;
        }
        else if(child->isTopState(new_state)) {
            top = true;
        }
        else {
            active_children++;

            constexpr double log_one_half = log(0.5);
            child_weight *= std::min(log(child_score), log(1.0 - child_score)) / log_one_half;
        }

        if (child_index == parity_child_index) {
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

        if (increase_score) {
            child_score = (0.75 + 0.25*child_score);
            child_weight *= 2.0;
        }
        if (decrease_score) {
            child_score = (0.25*child_score);
            child_weight *= 2.0;
        }
        score += child_score * child_weight;
        weights += child_weight;

        child_index++;
    }

    if (active_children == 0) {
        if (bottom && top) {
            setBottomState(new_state);
            return ColorScore(1 - parity_type, 0.0, 1.0);
        }
        else {
            setTopState(new_state);
            return ColorScore(parity_type, 1.0, 1.0);
        }
    }

    score /= weights;

    if (parity_child) {
        color_t color;
        const color_t c1 = child_colors[1 - parity_child_index];
        const color_t c2 = child_colors[parity_child_index];
        if (children[1 - parity_child_index]->node_type == NodeType::WEAK) {
            // one weak child
            color = c1 + c2;
        }
        else {
            // compute color
            if (c1 < d1) {
                color = c1 + std::min(c2, d2 - state[state_index + c1]);
            }
            else {
                color = c1 + c2;
            }

            // compute state update
            for (unsigned int i = 0; i < round_robin_size; i++) {
                if (c1 <= i) {
                    new_state[state_index + i] = 0;
                }
                else {
                    new_state[state_index + i] = d2 - std::min(c2, d2 - state[state_index + i]);
                }
            }
        }
        return ColorScore(color, score, weights);
    }
    else {
        // only weak children
        if (child_colors[0] == child_colors[1]) {
            return ColorScore(parity_type, score, weights);
        }
        else {
            return ColorScore(1 - parity_type, score, weights);
        }
    }
}

void ParityAutomatonTreeBiconditionalNode::setState(product_state_t& new_state, node_id_t state) {
    for (unsigned int i = 0; i < round_robin_size; i++) {
        new_state[state_index + i] = state;
    }
    children[0]->setState(new_state, state);
    children[1]->setState(new_state, state);
}

void ParityAutomatonTreeBiconditionalNode::setTopState(product_state_t& new_state) {
    if (round_robin_size > 0) {
        setState(new_state, NODE_NONE_TOP);
        new_state[state_index] = NODE_TOP;
    }
    else {
        // decide to set both to top
        children[0]->setTopState(new_state);
        children[1]->setTopState(new_state);
    }
}

void ParityAutomatonTreeBiconditionalNode::setBottomState(product_state_t& new_state) {
    if (round_robin_size > 0) {
        setState(new_state, NODE_NONE_BOTTOM);
        new_state[state_index] = NODE_BOTTOM;
    }
    else {
        // decide to set first to bottom and second to top
        children[0]->setBottomState(new_state);
        children[1]->setTopState(new_state);
    }
}

bool ParityAutomatonTreeBiconditionalNode::isTopState(const product_state_t& state) const {
    if (round_robin_size > 0) {
        return state[state_index] == NODE_TOP;
    }
    else {
        return children[0]->isTopState(state) && children[1]->isTopState(state);
    }
}
bool ParityAutomatonTreeBiconditionalNode::isBottomState(const product_state_t& state) const {
    if (round_robin_size > 0) {
        return state[state_index] == NODE_BOTTOM;
    }
    else {
        return children[0]->isBottomState(state) && children[1]->isTopState(state);
    }
}

}
