#pragma once

#include <queue>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include "owl.h"

#include "Definitions.h"

namespace aut {

enum NodeType : uint8_t {
    WEAK = 0,
    BUCHI = 1,
    CO_BUCHI = 2,
    PARITY = 3
};
inline NodeType join_node_type(const NodeType a, const NodeType b) {
    return (NodeType)(a | b);
}
inline NodeType join_node_type_biconditional(const NodeType a, const NodeType b) {
    if (join_node_type(a, b) == NodeType::WEAK) {
        return NodeType::WEAK;
    }
    else {
        return NodeType::PARITY;
    }
}

class Automaton {
    private:
        struct SuccessorCache {
            std::vector<int32_t> tree;
            std::vector<ScoredEdge> leaves;
            std::vector<ScoredEdge> direct;

            inline ScoredEdge tree_lookup(const letter_t letter) const {
                int32_t i = 0;
                if (!tree.empty()) {
                    do {
                        if ((letter & ((letter_t)1 << tree[i])) == 0) {
                            i = tree[i + 1];
                        } else {
                            i = tree[i + 2];
                        }
                    }
                    while (i > 0);
                }
                return leaves[-i];
            }

            inline ScoredEdge direct_lookup(const letter_t letter) const {
                return direct[letter];
            }

            inline ScoredEdge lookup(const letter_t letter) const {
                if (direct.empty()) {
                    return tree_lookup(letter);
                }
                else {
                    return direct_lookup(letter);
                }
            }

            void flatten_tree(const letter_t max_letter) {
                direct.reserve(max_letter);
                for (letter_t letter = 0; letter < max_letter; letter++) {
                    direct.push_back(tree_lookup(letter));
                }
            }
        };

        const owl::Automaton automaton;
        const NodeType node_type;
        const color_t max_color;
        const color_t default_color;
        const Parity parity_type;
        std::vector< SuccessorCache > successors;
        letter_t alphabet_size;
        letter_t max_number_successors;

        // queue for querying for successors
        std::queue<node_id_t> queries;

        // mutex for accessing the queue for queries
        std::mutex query_mutex;

        // mutex for accessing the successors
        std::mutex successors_mutex;

        // flag if the consumer thread currently has the lock on successors
        bool has_lock;

        // condition variable signalling new queries or complete construction
        std::condition_variable change;

        // condition variable signalling new successors
        std::condition_variable new_successors;

        // variable signalling that the automaton is completely constructed
        std::atomic<bool> complete;

        void add_new_query(node_id_t query);
        void mark_complete();
        void wait_for_query(node_id_t& query);
        void add_new_states();
        void add_successors(node_id_t local_state);

        color_t initMaxColor() const;
        color_t initDefaultColor() const;
        NodeType initNodeType() const;
        Parity initParityType() const;
        bool initSuccessorTree() const;

    public:
        Automaton(owl::Automaton automaton);

        void setAlphabetSize(const letter_t alphabet_size);

        ScoredEdge getSuccessor(node_id_t local_state, letter_t letter);

        color_t getMaxColor() const;
        NodeType getNodeType() const;
        Parity getParityType() const;

        void print_type() const;
        void print_memory_usage() const;
};

}
