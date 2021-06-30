#include "PGArena.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <cstring>
#include <limits>
#include <algorithm>
#include <cstdint>
#include <bitset>
#include <map>
#include <queue>
#include <memory>

#include <boost/functional/hash.hpp>

#include "util/Timer.h"

size_t hash_value(const Edge& edge) {
    return std::hash<Edge>()(edge);
}

size_t hash_value(const BDD& bdd) {
    return (size_t)bdd.getNode();
}

namespace pg {

constexpr size_t RESERVE = 4096;

PGArena::PGArena(const size_t n_inputs, const size_t n_outputs, aut::AutomatonTreeStructure& structure, const ExplorationStrategy exploration, const bool clear_queue) :
    structure(structure),
    exploration(exploration),
    clear_queue(clear_queue),
    winning_queue(0),
    unreachable_queue(0),
    n_inputs(n_inputs),
    n_outputs(n_outputs),
    complete(false),
    solved(false),
    parity_type(structure.getParityType()),
    n_colors(structure.getMaxColor() + 1),
    initial_node(0),
    n_env_nodes(0),
    n_sys_nodes(0),
    n_sys_edges(0),
    n_env_edges(0)
{
    // reserve some space in the vector to avoid initial resizing, which needs locking for the parallel construction
    sys_succs_begin.reserve(RESERVE);
    sys_succs.reserve(RESERVE);
    sys_output.reserve(RESERVE);
    env_succs_begin.reserve(RESERVE);
    env_succs.reserve(RESERVE);
    env_input.reserve(RESERVE);
    env_node_map.reserve(RESERVE);
    env_node_reachable.reserve(RESERVE);
    sys_winner.reserve(RESERVE);
    env_winner.reserve(RESERVE);
    winning_queue.reserve(RESERVE);
    unreachable_queue.reserve(RESERVE);

    sys_succs_begin.push_back(0);
    env_succs_begin.push_back(0);

    // initialize manager for BDDs
    manager_input_bdds = Cudd(n_inputs);
    manager_output_bdds = Cudd(n_outputs);
    manager_input_bdds.AutodynDisable();
    manager_output_bdds.AutodynDisable();

    // construct masks for unused and always true or false inputs and outputs
    relevant_inputs.reserve(n_inputs);
    relevant_outputs.reserve(n_outputs);
    unused_inputs_mask = 0;
    true_inputs_mask = 0;
    false_inputs_mask = 0;
    irrelevant_inputs_mask = 0;
    unused_outputs_mask = 0;
    true_outputs_mask = 0;
    false_outputs_mask = 0;
    irrelevant_outputs_mask = 0;

    std::vector<owl::VariableStatus> statuses = structure.getVariableStatuses();

    for (letter_t a = 0; a < n_inputs; a++) {
        const letter_t bit = ((letter_t)1 << a);
        int status = a >= statuses.size() ? owl::UNUSED : statuses[a];
        if (status == owl::USED) {
            relevant_inputs.push_back(a);
        }
        else {
            irrelevant_inputs_mask |= bit;
            if (status == owl::UNUSED) {
                unused_inputs_mask |= bit;
            }
            else if (status == owl::CONSTANT_TRUE) {
                true_inputs_mask |= bit;
            }
            else if (status == owl::CONSTANT_FALSE) {
                false_inputs_mask |= bit;
            }
            else {
                std::cerr << "Invalid status: " << status << std::endl;
                assert(false);
            }
        }
    }
    for (letter_t a = 0; a < n_outputs; a++) {
        const letter_t bit = ((letter_t)1 << a);
        const letter_t b = a + n_inputs;
        int status = b >= statuses.size() ? owl::UNUSED : statuses[b];
        if (status == owl::USED) {
            relevant_outputs.push_back(a);
        }
        else {
            irrelevant_outputs_mask |= bit;
            if (status == owl::UNUSED) {
                unused_outputs_mask |= bit;
            }
            else if (status == owl::CONSTANT_TRUE) {
                true_outputs_mask |= bit;
            }
            else if (status == owl::CONSTANT_FALSE) {
                false_outputs_mask |= bit;
            }
            else {
                std::cerr << "Invalid status: " << status << std::endl;
                assert(false);
            }
        }
    }

    n_env_actions = ((letter_t)1 << relevant_inputs.size());
    n_sys_actions = ((letter_t)1 << relevant_outputs.size());
}

PGArena::~PGArena() {
    // clean up BDDs before manager is destroyed
    std::vector<BDD>().swap(env_input);
    std::vector<BDD>().swap(sys_output);
}

template <typename Container>
struct container_hash {
    std::size_t operator()(Container const& c) const {
        return boost::hash_range(c.cbegin(), c.cend());
    }
};

int PGArena::computeStateLabels(std::vector<node_id_t>& visited_map, std::vector<int>& accumulated_bits) {
    // get visited states
    std::vector<std::set<node_id_t>> visited_states(product_state_size);
    for (node_id_t i = 0; i < visited_map.size(); i++) {
        if (visited_map[i] != NODE_NONE) {
            for (size_t j = 0; j < product_state_size; j++) {
                const node_id_t local_state = product_states[i][j];
                // none states are don't cares
                if (local_state != NODE_NONE && local_state != NODE_NONE_BOTTOM && local_state != NODE_NONE_TOP) {
                    visited_states[j].insert(product_states[i][j]);
                }
            }
        }
    }
    // get map for visited states
    std::vector<std::map<node_id_t, node_id_t>> inner_state_map(product_state_size);
    for (size_t i = 0; i < product_state_size; i++) {
        node_id_t new_id = 0;
        for (const auto& old_id : visited_states[i]) {
            inner_state_map[i].insert({ old_id, new_id });
            new_id++;
        }
    }

    accumulated_bits.resize(product_state_size + 1);
    // get number of bits
    int state_label_bits = 0;
    for (size_t i = 0; i < product_state_size; i++) {
        accumulated_bits[i] = state_label_bits;
        size_t state_size = visited_states[i].size();
        if (state_size > 1) {
            state_label_bits += log2(state_size - 1) + 1;
        }
    }
    accumulated_bits[product_state_size] = state_label_bits;
    constexpr int node_id_t_bitwidth = std::numeric_limits<node_id_t>::digits + std::numeric_limits<node_id_t>::is_signed;
    if (state_label_bits > node_id_t_bitwidth) {
        return -1;
    }

    // build state ids
    state_labels.resize(n_env_nodes);
    for (node_id_t i = 0; i < visited_map.size(); i++) {
        if (visited_map[i] != NODE_NONE) {
            node_id_t id_number = 0;
            node_id_t id_dontcare = 0;
            for (size_t j = 0; j < product_state_size; j++) {
                const node_id_t local_state = product_states[i][j];
                if (local_state != NODE_NONE && local_state != NODE_NONE_BOTTOM && local_state != NODE_NONE_TOP) {
                    id_number |= (inner_state_map[j][local_state] << accumulated_bits[j]);
                }
                else {
                    id_dontcare |= (((~((node_id_t)0)) << accumulated_bits[j]) & (((node_id_t)1 << accumulated_bits[j+1]) - 1));
                }
            }
            state_labels[i] = SpecSeq<node_id_t>(id_number, id_dontcare);
        }
    }

    return state_label_bits;
}

void PGArena::filter_queue(state_queue& queue, const std::vector<product_state_t>& states, std::unordered_set<node_id_t>& already_queried, const bool new_declared_nodes, std::chrono::duration<double>& time_query, size_t& queried_nodes, size_t& unreachable_nodes_found, size_t& losing_nodes_found, size_t& winning_nodes_found, const bool only_realizability) {
    std::chrono::high_resolution_clock::time_point start_time;
    std::chrono::high_resolution_clock::time_point stop_time;
    state_queue new_queue;
    for (ScoredProductState& s : queue.container()) {
        bool keep = true;
        if (env_node_map[s.ref_id] != NODE_NONE) {
            // node already explored
            keep = false;
        }
        else if (!env_node_reachable[s.ref_id]) {
            // node unreachable
            unreachable_nodes_found++;
            keep = false;
        }
        else if (new_declared_nodes) {
            auto result = already_queried.insert(s.ref_id);
            if (result.second) {
                //Player winner = Player::UNKNOWN;
                start_time = std::chrono::high_resolution_clock::now();
                queried_nodes++;
                Player winner = structure.queryWinner(states[s.ref_id]);
                stop_time = std::chrono::high_resolution_clock::now();
                time_query += (stop_time - start_time);

                if (winner == Player::ENV_PLAYER) {
                    // node unrealizable
                    losing_nodes_found++;
                    keep = false;
                    if (only_realizability) {
                        env_node_map[s.ref_id] = NODE_BOTTOM;
                    }
                    else {
                        // decrease score otherwise
                        s.score = 0.1*s.score;
                        new_queue.push(std::move(s));

                    }
                }
                else if (winner == Player::SYS_PLAYER) {
                    // node realizable
                    winning_nodes_found++;
                    keep = false;
                    if (only_realizability) {
                        env_node_map[s.ref_id] = NODE_TOP;
                    }
                    else {
                        // increase score otherwise
                        if (s.score >= 0.0) {
                            s.score = 0.9 + 0.1*s.score;
                        }
                        else {
                            s.score = -0.9 + 0.1*s.score;
                        }
                        new_queue.push(std::move(s));
                    }
                }
            }
        }
        if (keep) {
            new_queue.push(std::move(s));
        }
    }
    std::swap(new_queue, queue);
}

void PGArena::reachability_analysis() {
    // find unreachable nodes
    std::deque<node_id_t> queue;
    std::vector<bool> env_visited(n_env_nodes, false);
    std::vector<bool> sys_visited(n_sys_nodes, false);

    for (node_id_t u = 0; u < n_env_nodes; u++) {
        for (edge_id_t i = getEnvSuccsBegin(u); i != getEnvSuccsEnd(u); i++) {
            const node_id_t v = getEnvEdge(i);
            for (edge_id_t j = getSysSuccsBegin(v); j != getSysSuccsEnd(v); j++) {
                env_node_reachable[sys_succs[j].successor] = false;
            }
        }
    }

    env_node_reachable[initial_node_ref] = true;
    env_visited[initial_node] = true;
    queue.push_back(initial_node);

    while (!queue.empty()) {
        node_id_t u = queue.front();
        queue.pop_front();
        if (env_winner[u] == Player::UNKNOWN) {
            for (edge_id_t i = getEnvSuccsBegin(u); i != getEnvSuccsEnd(u); i++) {
                const node_id_t v = getEnvEdge(i);
                if (!sys_visited[v]) {
                    sys_visited[v] = true;
                    if (sys_winner[v] == Player::UNKNOWN) {
                        for (edge_id_t j = getSysSuccsBegin(v); j != getSysSuccsEnd(v); j++) {
                            const node_id_t w = getSysEdge(j).successor;
                            env_node_reachable[sys_succs[j].successor] = true;
                            if (w < n_env_nodes && !env_visited[w]) {
                                env_visited[w] = true;
                                queue.push_back(w);
                            }
                        }
                    }
                }
            }
        }
    }
}

void PGArena::constructArena(const bool parallel, const bool only_realizability, const int verbosity) {
    const product_state_t initial_state = structure.getInitialState();
    product_state_size = initial_state.size();
    bool has_lock = false;

    if (verbosity >= 1) {
        std::cout << "Product state tree:" << std::endl;
        structure.print(verbosity);
    }

    state_queue queue_max;
    state_queue queue_min;
    // map from product states in queue to scores and reference ids
    std::unordered_map< product_state_t, MinMaxState, container_hash<product_state_t> > state_map;
    std::vector<product_state_t> states;
    states.reserve(RESERVE);

    // cache for system nodes
    auto sys_node_hash = [this](const node_id_t sys_node) {
        const size_t begin = sys_succs_begin[sys_node];
        const size_t end = sys_succs_begin[sys_node + 1];
        size_t seed = 0;
        boost::hash_range(seed, sys_succs.cbegin() + begin, sys_succs.cbegin() + end);
        boost::hash_range(seed, sys_output.cbegin() + begin, sys_output.cbegin() + end);
        return seed;
    };
    auto sys_node_equal = [this](const node_id_t sys_node_1, const node_id_t sys_node_2) {
        const size_t begin1 = sys_succs_begin[sys_node_1];
        const size_t length1 = sys_succs_begin[sys_node_1 + 1] - begin1;
        const size_t begin2 = sys_succs_begin[sys_node_2];
        const size_t length2 = sys_succs_begin[sys_node_2 + 1] - begin2;
        if (length1 != length2) {
            return false;
        }
        else {
            bool result = true;
            for (size_t j = 0; j < length1; j++) {
                if (
                        (sys_succs[begin1 + j] != sys_succs[begin2 + j]) ||
                        (sys_output[begin1 + j] != sys_output[begin2 + j])
                ) {
                    result = false;
                    break;
                }
            }
            return result;
        }
    };
    auto sys_node_map = std::unordered_set<node_id_t, decltype(sys_node_hash), decltype(sys_node_equal)>(RESERVE, sys_node_hash, sys_node_equal);

    // map from memory ids (for solver) to ref ids (for looking up states)
    std::unordered_map<node_id_t, node_id_t> env_node_to_ref_id;

    // add ref for top node
    const node_id_t top_node_ref = env_node_map.size();
    env_node_map.push_back(NODE_TOP);
    env_node_reachable.push_back(true);
    states.push_back({});

    // add ref for initial node
    initial_node_ref = env_node_map.size();
    env_node_map.push_back(NODE_NONE);
    env_node_reachable.push_back(true);

    ScoredProductState initial(1.0, initial_node_ref);
    state_map.insert({ initial_state, MinMaxState(initial) });
    queue_max.push(initial);
    states.push_back(std::move(initial_state));

    bool use_max_queue = true;

    std::unordered_set<node_id_t> already_queried;
    bool new_winning_nodes = false;
    bool new_declared_nodes = false;

    std::chrono::high_resolution_clock::time_point start_time;
    std::chrono::high_resolution_clock::time_point stop_time;
    std::chrono::duration<double> time_query(0);
    std::chrono::duration<double> time_declare(0);
    std::chrono::duration<double> time_reach(0);
    size_t winning_nodes_found = 0;
    size_t losing_nodes_found = 0;
    size_t unreachable_nodes_found = 0;
    size_t queried_nodes = 0;

    while (!solved && !(queue_max.empty() && queue_min.empty())) {

        // check queue of winning nodes
        if (clear_queue) {
            int32_t winning;
            while (winning_queue.pop(winning)) {
                new_winning_nodes = true;

                node_id_t env_node = winning > 0 ? winning : -winning;
                Player winner = winning > 0 ? Player::SYS_PLAYER : Player::ENV_PLAYER;
                node_id_t ref_id = env_node_to_ref_id[env_node];
                if (ref_id == initial_node_ref) {
                    solved = true;
                    break;
                }
                else {
                    product_state_t& state = states[ref_id];
                    start_time = std::chrono::high_resolution_clock::now();
                    if (structure.declareWinning(state, winner)) {
                        new_declared_nodes = true;
                    }
                    stop_time = std::chrono::high_resolution_clock::now();
                    time_declare += (stop_time - start_time);
                }
            }
            if (solved) {
                break;
            }
        }

        // clear queue of non-reachable nodes and already winning nodes
        if (clear_queue && new_winning_nodes) {

            start_time = std::chrono::high_resolution_clock::now();
            reachability_analysis();
            stop_time = std::chrono::high_resolution_clock::now();
            time_reach += (stop_time - start_time);

            if (new_declared_nodes) {
                already_queried.clear();
            }

            if (exploration == ExplorationStrategy::BFS) {
                filter_queue(queue_max, states, already_queried, new_declared_nodes, time_query, queried_nodes, unreachable_nodes_found, losing_nodes_found, winning_nodes_found, only_realizability);
            }
            else if (exploration == ExplorationStrategy::PQ) {
                filter_queue(queue_max, states, already_queried, new_declared_nodes, time_query, queried_nodes, unreachable_nodes_found, losing_nodes_found, winning_nodes_found, only_realizability);
                filter_queue(queue_min, states, already_queried, new_declared_nodes, time_query, queried_nodes, unreachable_nodes_found, losing_nodes_found, winning_nodes_found, only_realizability);
            }

            new_winning_nodes = false;
            new_declared_nodes = false;
        }

        ScoredProductState scored_state;
        if (exploration == ExplorationStrategy::BFS) {
            scored_state = std::move(queue_max.top());
            queue_max.pop();
        }
        else if (exploration == ExplorationStrategy::PQ) {
            if (use_max_queue && !queue_max.empty()) {
                scored_state = std::move(queue_max.top());
                queue_max.pop();
                use_max_queue = false;
            }
            else if (!queue_min.empty()) {
                scored_state = std::move(queue_min.top());
                queue_min.pop();
                use_max_queue = true;
            }
            else {
                use_max_queue = !use_max_queue;
                continue;
            }
        }

        const node_id_t ref_id = scored_state.ref_id;
        if (env_node_map[ref_id] != NODE_NONE) {
            // node already explored
            continue;
        }

        const node_id_t env_node = n_env_nodes;
        env_node_map[ref_id] = env_node;
        env_node_to_ref_id.insert({ env_node, ref_id });

        if (verbosity >= 1) {
            std::cout << " [" << std::setw(4) << env_node << "] Computing successors for " << std::setw(4) << ref_id;
            std::cout << " (" << std::fixed << std::setprecision(3) << std::setw(6) << std::abs(scored_state.score) << ") = (";
            for (const auto s : states[ref_id]) {
                if (s == NODE_TOP) {
                    std::cout << "  ⊤";
                }
                else if (s == NODE_BOTTOM) {
                    std::cout << "  ⊥";
                }
                else if (s == NODE_NONE) {
                    std::cout << "  -";
                }
                else if (s == NODE_NONE_TOP) {
                    std::cout << " -⊤";
                }
                else if (s == NODE_NONE_BOTTOM) {
                    std::cout << " -⊥";
                }
                else {
                    std:: cout << " " << std::setw(2) << s;
                }
            }
            std::cout << " )";
            if (verbosity >= 3) {
                std::cout << " : [";
            }
            else {
                std::cout << std::endl;
            }
        }

        edge_id_t cur_env_node_n_sys_edges = 0;
        node_id_t cur_n_sys_nodes = 0;

        std::map<node_id_t, BDD> env_successors;

        for (letter_t i = 0; i < n_env_actions; i++) {
            // compute input letter
            letter_t relevant_input = 0;
            for (size_t b = 0; b < relevant_inputs.size(); b++) {
                relevant_input |= ((i & ((letter_t)1 << b)) >> b) << relevant_inputs[b];
            }
            SpecSeq<letter_t> input_letter(relevant_input, irrelevant_inputs_mask);

            // new sys node with successors
            node_id_t sys_node = n_sys_nodes + cur_n_sys_nodes;
            std::map<Edge, BDD> sys_successors;
            edge_id_t cur_sys_node_n_sys_edges = 0;

            for (letter_t o = 0; o < n_sys_actions; o++) {
                // compute output letter
                letter_t relevant_output = 0;
                for (size_t b = 0; b < relevant_outputs.size(); b++) {
                    relevant_output |= ((o & ((letter_t)1 << b)) >> b) << relevant_outputs[b];
                }
                SpecSeq<letter_t> output_letter(relevant_output, irrelevant_outputs_mask);

                // compute joint letter for automata lookup
                letter_t letter = input_letter.number + (output_letter.number << n_inputs);

                product_state_t new_state(product_state_size);
                const ColorScore cs = structure.getSuccessor(states[ref_id], new_state, letter);
                const color_t color = cs.color;

                node_id_t succ = env_node_map.size();
                double score;

                if (exploration == ExplorationStrategy::PQ) {
                    score = cs.score;
                    // decrease score for nodes discovered later to mix in BFS aspect
                    constexpr double factor = 1.0 - pow(0.5, 6);
                    score *= pow(factor, (double)(env_node / 100));
                }
                else { // exploration == ExplorationStrategy::BFS
                    score = -(double)succ;
                }

                if (!structure.isBottomState(new_state)) {
                    if (structure.isTopState(new_state)) {
                        succ = top_node_ref;
                    }
                    else {
                        auto result = state_map.insert({ new_state, MinMaxState(score, succ) });
                        if (result.second) {
                            // new successor
                            if (
                                    env_node_map.size() == env_node_map.capacity()
                            ) {
                                arena_mutex.lock();
                                has_lock = true;
                            }
                            env_node_map.push_back(NODE_NONE);
                            if (has_lock) {
                                arena_mutex.unlock();
                                has_lock = false;
                            }
                            env_node_reachable.push_back(true);

                            states.push_back(std::move(new_state));

                            if (exploration == ExplorationStrategy::BFS) {
                                queue_max.push(ScoredProductState( score, succ));
                            }
                            else if (exploration == ExplorationStrategy::PQ) {
                                queue_max.push(ScoredProductState( score, succ));
                                queue_min.push(ScoredProductState(-score, succ));
                            }
                        }
                        else {
                            // successor already seen
                            succ = result.first->second.ref_id;
                            double& succ_max_score = result.first->second.max_score;
                            double& succ_min_score = result.first->second.min_score;

                            if (clear_queue && env_node_map[succ] == NODE_NONE && !env_node_reachable[succ]) {
                                // node may have been removed from queue, need to add it again
                                env_node_reachable[succ] = true;
                                if (exploration == ExplorationStrategy::BFS) {
                                    queue_max.push(ScoredProductState(-((double)succ) , succ));
                                }
                                else if (exploration == ExplorationStrategy::PQ) {
                                    queue_max.push(ScoredProductState( score, succ));
                                    queue_min.push(ScoredProductState(-score, succ));
                                }
                            }
                            else if (exploration == ExplorationStrategy::PQ && (score < succ_min_score || score > succ_max_score)) {
                                // score of node in queue changed
                                if (score > succ_max_score) {
                                    succ_max_score = score;
                                    queue_max.push(ScoredProductState( score, succ));
                                }
                                else if (score < succ_min_score) {
                                    succ_min_score = score;
                                    queue_min.push(ScoredProductState(-score, succ));
                                }
                            }
                        }
                    }

                    const node_id_t succ_node = env_node_map[succ];
                    if (succ_node == NODE_BOTTOM) {
                        // successor losing, do not add an edge
                    }
                    else {
                        Edge edge(succ, color);

                        if (true || !only_realizability) {
                            // add output to bdd
                            BDD output_bdd = output_letter.toBDD(manager_output_bdds, n_outputs);

                            auto const result = sys_successors.insert({ edge, output_bdd });
                            if (!result.second) {
                                result.first->second |= output_bdd;
                            }
                        }
                        else {
                            sys_successors[edge] = manager_output_bdds.bddOne();
                        }
                    }
                }
            }

            if (verbosity >= 3) {
                for (const auto& it : sys_successors) {
                    if (it.first.successor == top_node_ref) {
                        std::cout << "   ⊤   ";
                    }
                    else {
                        std::cout << " " << std::setw(2) << it.first.successor;
                        std::cout << ":" << std::setw(2) << it.first.color;
                        std::cout << " ";
                    }
                }
            }

            cur_sys_node_n_sys_edges = sys_successors.size();

            if (parallel && (
                    (sys_succs.size() + cur_sys_node_n_sys_edges > sys_succs.capacity()) ||
                    (sys_succs_begin.size() == sys_succs_begin.capacity()) ||
                    (sys_winner.size() == sys_winner.capacity())
            )) {
                arena_mutex.lock();
                has_lock = true;
            }
            for (const auto& it : sys_successors) {
                sys_succs.push_back(it.first);
                sys_output.push_back(it.second);
            }
            sys_succs_begin.push_back(sys_succs.size());
            sys_winner.push_back(Player::UNKNOWN);

            if (has_lock) {
                arena_mutex.unlock();
                has_lock = false;
            }

            auto const result = sys_node_map.insert(sys_node);
            if (result.second) {
                // new sys node
                cur_n_sys_nodes++;
            }
            else {
                // sys node already present
                sys_succs.resize(sys_succs.size() - cur_sys_node_n_sys_edges);
                sys_output.resize(sys_output.size() - cur_sys_node_n_sys_edges);
                sys_succs_begin.resize(sys_succs_begin.size() - 1);
                sys_winner.resize(sys_winner.size() - 1);
                cur_sys_node_n_sys_edges = 0;

                sys_node = *result.first;
            }

            if (true || !only_realizability) {
                // add input to bdd
                BDD input_bdd = input_letter.toBDD(manager_input_bdds, n_inputs);
                auto const result = env_successors.insert({ sys_node, input_bdd });
                if (!result.second) {
                    result.first->second |= input_bdd;
                }
            }
            else {
                env_successors[sys_node] = manager_output_bdds.bddOne();
            }

            cur_env_node_n_sys_edges += cur_sys_node_n_sys_edges;
        }

        if (parallel && (
                (env_succs.size() + env_successors.size() > env_succs.capacity()) ||
                (env_succs_begin.size() == env_succs_begin.capacity()) ||
                (env_winner.size() == env_winner.capacity())
        )) {
            arena_mutex.lock();
            has_lock = true;
        }
        n_env_edges += env_successors.size();
        for (const auto& it : env_successors) {
            env_succs.push_back(it.first);
            env_input.push_back(it.second);
        }
        env_succs_begin.push_back(env_succs.size());
        env_winner.push_back(Player::UNKNOWN);

        if (has_lock) {
            arena_mutex.unlock();
            has_lock = false;
        }

        if (verbosity >= 3) {
            std::cout << "]" << std::endl;
        }

        if (parallel) {
            size_mutex.lock();
        }
        n_sys_edges += cur_env_node_n_sys_edges;
        n_sys_nodes += cur_n_sys_nodes;
        n_env_nodes++;
        if (parallel) {
            size_mutex.unlock();
            change.notify_all();
        }
    }

    assert(n_sys_nodes + 1 == sys_succs_begin.size());
    assert(n_env_nodes + 1 == env_succs_begin.size());
    assert(n_sys_nodes == sys_winner.size());
    assert(n_env_nodes == env_winner.size());
    assert(n_sys_edges == sys_succs.size());
    assert(n_env_edges == env_succs.size());

    if (verbosity >= 2) {
        std::cout << " * Reachability analysis took " << std::fixed << std::setprecision(2) << time_reach.count() << " seconds." << std::endl;
        std::cout << " * Queries took " << std::fixed << std::setprecision(2) << time_query.count() << " seconds." << std::endl;
        std::cout << " * Declarations took " << std::fixed << std::setprecision(2) << time_declare.count() << " seconds." << std::endl;
        std::cout << " * Queried nodes: " << queried_nodes << std::endl;
        std::cout << " * Unreachable nodes found: " << unreachable_nodes_found << std::endl;
        std::cout << " * Winning nodes found: " << winning_nodes_found << std::endl;
        std::cout << " * Losing nodes found: " << losing_nodes_found << std::endl;
    }

    // notify solver that arena is completely constructed
    complete = true;
    change.notify_all();

    // fill vector for product states
    product_states.resize(n_env_nodes);
    for (auto& entry : state_map) {
        const node_id_t node_id = env_node_map[entry.second.ref_id];
        if (node_id != NODE_NONE && node_id != NODE_BOTTOM && node_id != NODE_TOP) {
            product_states[node_id] = std::move(entry.first);
        }
    }

    // clear structure, not needed any more
    //structure.clear();
}

void PGArena::print_basic_info() const {
    std::cout << "Number of env nodes  : " << n_env_nodes << std::endl;
    std::cout << "Number of sys nodes  : " << n_sys_nodes << std::endl;
    std::cout << "Number of env edges  : " << n_env_edges << std::endl;
    std::cout << "Number of sys edges  : " << n_sys_edges << std::endl;
    std::cout << "Number of colors     : " << n_colors << std::endl;
}

void PGArena::print(std::ostream& out, Player winner) const {
    size_t total_nodes = n_env_nodes + n_sys_nodes + n_sys_edges + 3;
    size_t boundary_node = total_nodes - 1;
    size_t bottom_node = total_nodes - 2;
    size_t top_node = total_nodes - 3;
    color_t max_color = n_colors - 1;
    if (max_color % 2 != 0) {
        max_color++;
    }
    color_t neutral_color = 0;

    size_t id_size = 1 + (size_t)log10(total_nodes);
    size_t color_size = 1 + (size_t)log10(max_color);

    out << "parity " <<  + total_nodes << ";" << std::endl;

    // environment nodes
    for (node_id_t i = 0; i < n_env_nodes; i++) {
        out << std::setw(id_size) << i << " "
            << std::setw(color_size) << neutral_color << " "
            << (1 - parity_type) << " ";

        for (edge_id_t j = getEnvSuccsBegin(i); j != getEnvSuccsEnd(i); j++) {
            const node_id_t successor = getEnvEdge(j);
            out << std::setw(id_size) << (successor + n_env_nodes);
            if (j + 1 != getEnvSuccsEnd(i)) {
                out << ",";
            }
        }
        out << " \"env: ";
        for (edge_id_t j = getEnvSuccsBegin(i); j != getEnvSuccsEnd(i); j++) {
            out << getEnvInput(j);
            if (j + 1 != getEnvSuccsEnd(i)) {
                out << ", ";
            }
        }
        out << "\"";
        out << ";" << std::endl;
    }
    out << std::endl;

    // system nodes
    for (node_id_t i = 0; i < n_sys_nodes; i++) {
        out << std::setw(id_size) << (i + n_env_nodes) << " "
            << std::setw(color_size) << neutral_color << " "
            << parity_type << " ";

        if (getSysSuccsBegin(i) == getSysSuccsEnd(i)) {
            out << std::setw(id_size) << bottom_node << " \"sys: true\"";
        }
        else {
            for (edge_id_t j = getSysSuccsBegin(i); j != getSysSuccsEnd(i); j++) {
                out << std::setw(id_size) << (j + n_env_nodes + n_sys_nodes);
                if (j + 1 != getSysSuccsEnd(i)) {
                    out << ",";
                }
            }
            out << " \"sys: ";
            for (edge_id_t j = getSysSuccsBegin(i); j != getSysSuccsEnd(i); j++) {
                out << getSysOutput(j);
                if (j + 1 != getSysSuccsEnd(i)) {
                    out << ", ";
                }
            }
            out << "\"";
        }
        out << ";" << std::endl;

        for (edge_id_t j = getSysSuccsBegin(i); j != getSysSuccsEnd(i); j++) {
            const Edge edge = getSysEdge(j);
            const node_id_t successor = edge.successor;
            const color_t color = edge.color;

            out << std::setw(id_size) << (j + n_env_nodes + n_sys_nodes) << " "
                << std::setw(color_size) << (max_color - color) << " "
                << parity_type << " ";

            if (successor == NODE_TOP) {
                out << std::setw(id_size) << top_node;
            }
            else if (successor == NODE_BOTTOM) {
                out << std::setw(id_size) << bottom_node;
            }
            else if (successor >= n_env_nodes) {
                out << std::setw(id_size) << boundary_node;
            }
            else {
                out << std::setw(id_size) << successor;
            }
            out << " \"sys: true\";" << std::endl;
        }
    }
    out << std::endl;

    // top, bottom and unexplored nodes
    out << std::setw(id_size) << top_node << " "
        << std::setw(color_size) << parity_type << " " << parity_type << " "
        << top_node << " \"top\";" << std::endl;
    out << std::setw(id_size) << bottom_node << " "
        << std::setw(color_size) << (1 - parity_type) << " " << (1 - parity_type) << " "
        << bottom_node << " \"bottom\";" << std::endl;
    out << std::setw(id_size) << boundary_node << " ";
    if (winner == SYS_PLAYER) {
        out << std::setw(color_size) << (1 - parity_type) << " " << (1 - parity_type) << " ";
    }
    else if (winner == ENV_PLAYER) {
        out << std::setw(color_size) << parity_type << " " << parity_type << " ";
    }
    else {
        out << std::setw(color_size) << "-" << " - ";
    }
    out << boundary_node << " \"unexplored\";" << std::endl;
}

}
