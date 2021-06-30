#pragma once

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <unordered_set>

#include "cuddObj.hh"
#include <boost/lockfree/queue.hpp>

#include "Definitions.h"
#include "util/Quine.h"
#include "util/SpecSeq.h"
#include "aut/ParityAutomatonTree.h"

namespace pg {

class PGArena {
private:
    aut::AutomatonTreeStructure& structure;
    const ExplorationStrategy exploration;
    const bool clear_queue;

    std::vector<letter_t> relevant_inputs;
    std::vector<letter_t> relevant_outputs;

    letter_t unused_inputs_mask;
    letter_t unused_outputs_mask;
    letter_t true_inputs_mask;
    letter_t true_outputs_mask;
    letter_t false_inputs_mask;
    letter_t false_outputs_mask;
    letter_t irrelevant_inputs_mask;
    letter_t irrelevant_outputs_mask;

    std::vector<BDD> sys_output;
    std::vector<edge_id_t> sys_succs_begin;
    std::vector<Edge> sys_succs;

    std::vector<BDD> env_input;
    std::vector<edge_id_t> env_succs_begin;
    std::vector<node_id_t> env_succs;

    std::vector<node_id_t> env_node_map;
    std::vector<bool> env_node_reachable;

    std::vector<Player> sys_winner;
    std::vector<Player> env_winner;

    size_t product_state_size;
    std::vector<product_state_t> product_states;
    std::vector<SpecSeq<node_id_t>> state_labels;

    boost::lockfree::queue<int32_t> winning_queue;
    boost::lockfree::queue<uint64_t> unreachable_queue;

    // Manager for BDDs of inputs and outputs
    Cudd manager_input_bdds;
    Cudd manager_output_bdds;

    struct ScoredProductState {
        double score;
        node_id_t ref_id;

        ScoredProductState() :
            score(0.0),
            ref_id(NODE_NONE)
        {}
        ScoredProductState(double score, node_id_t ref_id) :
            score(score),
            ref_id(ref_id)
        {}
    };

    struct MinMaxState {
        double min_score;
        double max_score;
        node_id_t ref_id;

        MinMaxState(const ScoredProductState& product_state) :
            min_score(product_state.score),
            max_score(product_state.score),
            ref_id(product_state.ref_id)
        {}
        MinMaxState(double score, node_id_t ref_id) :
            min_score(score),
            max_score(score),
            ref_id(ref_id)
        {}
        MinMaxState(double min_score, double max_score, node_id_t ref_id) :
            min_score(min_score),
            max_score(max_score),
            ref_id(ref_id)
        {}
    };

    struct ScoredProductStateComparator {
        bool operator() (ScoredProductState& s1, ScoredProductState& s2) {
            return s1.score < s2.score;
        }
    };
    template <class T, class Container = std::vector<T>, class Compare = std::less<T> >
    class PQ : public std::priority_queue<T, Container, Compare> {
    public:
        Container& container() { return this->c; }
    };
    typedef PQ<ScoredProductState, std::deque<ScoredProductState>, ScoredProductStateComparator> state_queue;

    void filter_queue(state_queue& queue, const std::vector<product_state_t>& states, std::unordered_set<node_id_t>& already_queried, const bool new_winning_nodes, std::chrono::duration<double>& time_query, size_t& queried_nodes, size_t& unreachable_found, size_t& losing_nodes_found, size_t& winning_nodes_found, const bool only_realizability);

    void reachability_analysis();

public:
    const size_t n_inputs;
    const size_t n_outputs;

    PGArena(const size_t n_inputs, const size_t n_outputs, aut::AutomatonTreeStructure& structure, const ExplorationStrategy exploration, const bool clear_queue);
    ~PGArena();

    void constructArena(const bool parallel = false, const bool only_realizability = false, const int verbosity = 0);
    int computeStateLabels(std::vector<node_id_t>& visited_map, std::vector<int>& accumulated_bits);

    // mutex for reading from or resizing the arena vectors
    std::mutex arena_mutex;

    // mutex for accessing size of the arena
    std::mutex size_mutex;

    // condition variable signalling change in size of the arena
    std::condition_variable change;

    // variable signalling that the arena is completely constructed
    std::atomic<bool> complete;

    // variable signalling that the arena is already solved
    std::atomic<bool> solved;

    // variable signalling that the queue should be cleared
    std::atomic<bool> signal_clear_queue;

    const Parity parity_type;
    const color_t n_colors;

    const node_id_t initial_node;
    node_id_t initial_node_ref;

    letter_t n_env_actions;
    letter_t n_sys_actions;

    std::atomic<node_id_t> n_env_nodes;
    std::atomic<node_id_t> n_sys_nodes;

    std::atomic<edge_id_t> n_sys_edges;
    std::atomic<edge_id_t> n_env_edges;

    void print(std::ostream& out, Player winner = UNKNOWN) const;
    void print_basic_info() const;

    inline Player getSysWinner(node_id_t sys_node) const {
        return sys_winner[sys_node];
    }
    inline Player getEnvWinner(node_id_t env_node) const {
        return env_winner[env_node];
    }
    inline void setSysWinner(node_id_t sys_node, Player winner) {
        sys_winner[sys_node] = winner;
    }
    inline void setEnvWinner(node_id_t env_node, Player winner) {
        env_winner[env_node] = winner;
        if (clear_queue) {
            winning_queue.push((int32_t)winner*(int32_t)env_node);
        }
    }

    inline edge_id_t getSysSuccsBegin(node_id_t sys_node) const { return sys_succs_begin[sys_node]; }
    inline edge_id_t getSysSuccsEnd(node_id_t sys_node) const { return sys_succs_begin[sys_node + 1]; }
    inline Edge getSysEdge(edge_id_t sys_edge) const {
        return Edge(env_node_map[sys_succs[sys_edge].successor], sys_succs[sys_edge].color);
    }
    inline BDD getSysOutput(edge_id_t sys_edge) const { return sys_output[sys_edge]; }

    inline edge_id_t getEnvSuccsBegin(node_id_t env_node) const { return env_succs_begin[env_node]; };
    inline edge_id_t getEnvSuccsEnd(node_id_t env_node) const { return env_succs_begin[env_node + 1]; };
    inline node_id_t getEnvEdge(edge_id_t env_edge) const { return env_succs[env_edge]; };
    inline BDD getEnvInput(edge_id_t env_edge) const { return env_input[env_edge]; }

    inline BDD anyOutput() const { return manager_output_bdds.bddOne(); }
    inline BDD noOutput() const { return manager_output_bdds.bddZero(); }
    inline BDD anyInput() const { return manager_input_bdds.bddOne(); }
    inline BDD noInput() const { return manager_input_bdds.bddZero(); }
    inline SpecSeq<letter_t> addUnrealizableInputMask(SpecSeq<letter_t> input) const {
        return SpecSeq<letter_t>(input.number | true_inputs_mask, input.unspecifiedBits & ~(true_inputs_mask | false_inputs_mask));
    }
    inline SpecSeq<letter_t> addRealizableOutputMask(SpecSeq<letter_t> output) const {
        return SpecSeq<letter_t>(output.number | true_outputs_mask, output.unspecifiedBits & ~(true_outputs_mask | false_outputs_mask));
    }

    inline SpecSeq<node_id_t> getStateLabel(node_id_t env_node) const {
        return state_labels[env_node];
    };
};

}
