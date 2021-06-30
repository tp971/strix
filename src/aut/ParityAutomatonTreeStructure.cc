#include "ParityAutomatonTree.h"

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cmath>

namespace aut {

bool cmp_trees(const std::unique_ptr<ParityAutomatonTree>& t1, const std::unique_ptr<ParityAutomatonTree>& t2) {
    if (t1->node_type < t2->node_type) {
        return true;
    }
    else if (t1->node_type > t2->node_type) {
        return false;
    }
    else {
        const letter_t size1 = t1->getMaximumAlphabetSize();
        const letter_t size2 = t2->getMaximumAlphabetSize();
        if (size1 < size2) {
            return true;
        }
        else if (size1 > size2) {
            return false;
        }
        else {
            const auto alphabet1 = t1->getAlphabet();
            const auto alphabet2 = t2->getAlphabet();
            if (alphabet1 < alphabet2) {
                return true;
            }
            else if (alphabet1 > alphabet2) {
                return false;
            }
            else {
                return t1->getMinIndex() < t2->getMinIndex();
            }
        }
    }
}

AutomatonTreeStructure::AutomatonTreeStructure(owl::DecomposedDPA automaton) :
    owl_automaton(std::move(automaton))
{
    std::vector<owl::Automaton> owl_automata = owl_automaton.automata();
    auto it = owl_automata.begin();
    auto it_end = owl_automata.end();
    it += automata.size();
    while (it != it_end) {
        automata.emplace_back(std::move(*it));
        it++;
    }
    const std::unique_ptr<owl::LabelledTree<owl::Tag, owl::Reference>> structure = owl_automaton.structure();

    std::vector<ParityAutomatonTreeLeaf*> leaves;
    tree = constructTree(structure, leaves);
    tree->getInitialState(initial_state);
    for (const ParityAutomatonTreeLeaf* leaf : leaves) {
        leaf_state_indices.push_back(leaf->getStateIndex());
    }
}

AutomatonTreeStructure::~AutomatonTreeStructure() {
    clear();
}

std::unique_ptr<ParityAutomatonTree> AutomatonTreeStructure::constructTree(const std::unique_ptr<owl::LabelledTree<owl::Tag, owl::Reference>>& tree, std::vector<ParityAutomatonTreeLeaf*>& leaves) {
    if (tree->is_leaf()) {
        const owl::Reference& reference = tree->label2();
        Automaton& automaton = automata[reference.index];
        ParityAutomatonTreeLeaf* leaf = new ParityAutomatonTreeLeaf(automaton, std::move(reference));
        leaves.push_back(leaf);
        return std::unique_ptr<ParityAutomatonTree>(leaf);
    }
    else {
        const owl::Tag tag = tree->label1();

        // defaults for weak nodes
        NodeType node_type = NodeType::WEAK;
        Parity parity_type = Parity::EVEN;
        color_t max_color = 1;
        node_id_t round_robin_size = 0;

        int parity_child = false;
        color_t parity_child_max_color = 0;
        Parity parity_child_parity_type = Parity::EVEN;
        int parity_child_index = 0;

        std::vector<std::unique_ptr<ParityAutomatonTree>> children;
        for (const std::unique_ptr<owl::LabelledTree<owl::Tag, owl::Reference>>& child : tree->children()) {
            std::unique_ptr<ParityAutomatonTree> pchild = constructTree(child, leaves);
            if (pchild->node_type == NodeType::PARITY) {
                if (tag == owl::BICONDITIONAL) {
                    parity_child = true;
                }
                else if (parity_child) {
                    throw std::invalid_argument("unsupported automaton tree");
                }
                else {
                    parity_child = true;
                    parity_child_parity_type = pchild->parity_type;
                    parity_child_max_color = pchild->max_color;
                }
            }
            else if (pchild->node_type == NodeType::BUCHI) {
                if (tag == owl::CONJUNCTION) {
                    round_robin_size++;
                }
                else if (tag == owl::BICONDITIONAL) {
                    parity_child = true;
                }
            }
            else if (pchild->node_type == NodeType::CO_BUCHI) {
                if (tag == owl::DISJUNCTION) {
                    round_robin_size++;
                }
                else if (tag == owl::BICONDITIONAL) {
                    parity_child = true;
                }
            }

            node_type = join_node_type(node_type, pchild->node_type);

            children.push_back(std::move(pchild));
        }

        // sort children from "small,simple" to "large,complex" as a heuristic for more efficient product construction
        std::sort(children.begin(), children.end(), cmp_trees);

        if (tag == owl::BICONDITIONAL) {
            const NodeType t1 = children[0]->node_type;
            const NodeType t2 = children[1]->node_type;
            node_type = join_node_type_biconditional(t1, t2);

            if (parity_child) {
                // need to treat one child as a parity child and other as child updating the lar

                if (t1 == NodeType::WEAK) {
                    parity_child_index = 1;
                }
                else if (t2 == NodeType::WEAK) {
                    parity_child_index = 0;
                }
                else if (children[0]->max_color < children[1]->max_color) {
                    parity_child_index = 1;
                }
                else {
                    parity_child_index = 0;
                }
                parity_child_parity_type = children[parity_child]->parity_type;
                parity_child_max_color = children[parity_child]->max_color;
            }
        }

        if (node_type == NodeType::PARITY) {
            if (tag == owl::CONJUNCTION || tag == owl::DISJUNCTION) {
                if (tag == owl::CONJUNCTION) {
                    parity_type = Parity::ODD;
                }
                else {
                    parity_type = Parity::EVEN;
                }
                if (parity_child) {
                    if (parity_type != parity_child_parity_type) {
                        parity_child_max_color++;
                    }
                    max_color = parity_child_max_color;
                    if (round_robin_size > 0 && max_color % 2 != 0) {
                        max_color++;
                    }
                }
                else {
                    // needs to have co-buchi and buchi children
                    max_color = 2;
                }
            }
            else {
                if (parity_child) {
                    color_t d1 = children[1 - parity_child_index]->max_color;
                    color_t d2 = children[parity_child_index]->max_color;
                    Parity p1 = children[1 - parity_child_index]->parity_type;
                    Parity p2 = children[parity_child_index]->parity_type;

                    if (children[1 - parity_child_index]->node_type == NodeType::WEAK) {
                        // one weak child
                        max_color = d2 + 1;
                        parity_type = p2;
                        round_robin_size = 0;
                    }
                    else {
                        max_color = d1 + d2;
                        parity_type = (Parity)(((int)p1 + (int)p2) % 2);
                        round_robin_size = d1;
                    }
                    parity_child_max_color = d2;
                }
                // only weak children otherwise, leave defaults
            }
        }
        else if (node_type == NodeType::BUCHI) {
            parity_type = Parity::EVEN;
        }
        else if (node_type == NodeType::CO_BUCHI) {
            parity_type = Parity::ODD;
        }

        if (tag == owl::CONJUNCTION || tag == owl::DISJUNCTION) {
            return std::unique_ptr<ParityAutomatonTree>(
                    new ParityAutomatonTreeNode(
                        tag, node_type, parity_type, max_color, round_robin_size, parity_child, parity_child_max_color, std::move(children)));
        }
        else {
            return std::unique_ptr<ParityAutomatonTree>(
                    new ParityAutomatonTreeBiconditionalNode(
                        node_type, parity_type, max_color, round_robin_size, parity_child, parity_child_max_color,
                        parity_child_index, std::move(children)));
        }
    }
}

Parity AutomatonTreeStructure::getParityType() const {
    return tree->parity_type;
}

color_t AutomatonTreeStructure::getMaxColor() const {
    return tree->max_color;
}

product_state_t AutomatonTreeStructure::getInitialState() const {
    return initial_state;
}

ColorScore AutomatonTreeStructure::getSuccessor(const product_state_t& state, product_state_t& new_state, letter_t letter) {
    return tree->getSuccessor(state, new_state, letter);
}

std::vector<jint> AutomatonTreeStructure::getAutomatonStates(const product_state_t& state) const {
    std::vector<jint> automaton_states;
    automaton_states.reserve(leaf_state_indices.size());
    for (size_t index : leaf_state_indices) {
        node_id_t local_state = state[index];
        jint owl_state;
        if (local_state == NODE_NONE) {
            throw std::invalid_argument("local state should never be NONE");
        }
        else if (local_state == NODE_BOTTOM || local_state == NODE_NONE_BOTTOM) {
            owl_state = -1;
        }
        else if (local_state == NODE_TOP || local_state == NODE_NONE_TOP) {
            owl_state = -2;
        }
        else {
            owl_state = local_state;
        }

        automaton_states.push_back(owl_state);
    }

    return automaton_states;
}

bool AutomatonTreeStructure::declareWinning(const product_state_t& state, const Player winner) {
    std::vector<jint> automaton_states = getAutomatonStates(state);
    switch (winner) {
        case Player::SYS_PLAYER:
            return owl_automaton.declare(owl::RealizabilityStatus::REALIZABLE, std::move(automaton_states));
        case Player::ENV_PLAYER:
            return owl_automaton.declare(owl::RealizabilityStatus::UNREALIZABLE, std::move(automaton_states));
        default:
            return false;
    }
}

Player AutomatonTreeStructure::queryWinner(const product_state_t& state) {
    std::vector<jint> automaton_states = getAutomatonStates(state);
    owl::RealizabilityStatus status = owl_automaton.query(std::move(automaton_states));
    switch (status) {
        case owl::RealizabilityStatus::REALIZABLE:
            return Player::SYS_PLAYER;
        case owl::RealizabilityStatus::UNREALIZABLE:
            return Player::ENV_PLAYER;
        default:
            return Player::UNKNOWN;
    }
}

bool AutomatonTreeStructure::isTopState(const product_state_t& state) const {
    return tree->isTopState(state);
}

bool AutomatonTreeStructure::isBottomState(const product_state_t& state) const {
    return tree->isBottomState(state);
}

std::set<letter_t> AutomatonTreeStructure::getAlphabet() const {
    return tree->getAlphabet();
}

std::vector<owl::VariableStatus> AutomatonTreeStructure::getVariableStatuses() const {
    return owl_automaton.variable_statuses();
}

void AutomatonTreeStructure::clear() {
    automata.clear();
    // free memory from automata and tree
    std::deque<Automaton>().swap(automata);
    tree.reset();
}

void AutomatonTreeStructure::print(const int verbosity) const {
    tree->print(verbosity);
}

void AutomatonTreeStructure::print_memory_usage() const {
    for (const auto& automaton : automata) {
        automaton.print_memory_usage();
    }
}

}
