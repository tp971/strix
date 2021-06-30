#include "PGSolver.h"

#include <bitset>
#include <deque>
#include <set>
#include <mutex>

#include "omp.h"
#include "cuddObj.hh"

namespace pg {

PGSolver::PGSolver(pg::PGArena& arena, const bool onthefly_construction, const int num_threads, const bool compact_colors, const int verbosity) :
    arena(arena),
    onthefly_construction(onthefly_construction),
    num_threads(num_threads),
    compact_colors(compact_colors),
    verbosity(verbosity),
    winner(UNKNOWN),
    n_colors(arena.n_colors)
{
    init_solver();
}

PGSolver::~PGSolver() { }

void PGSolver::init_solver() {
    n_env_nodes = arena.n_env_nodes;
    n_env_edges = arena.n_env_edges;
    n_sys_edges = arena.n_sys_edges;
    n_sys_nodes = arena.n_sys_nodes;
}

void PGSolver::preprocess_and_solve_game() {
    if (compact_colors) {
        reduce_colors();
    }
    else {
        copy_colors();
    }
    solve_game();
}


void PGSolver::reduce_colors() {
    n_colors = arena.n_colors;

    std::vector<edge_id_t> color_count(n_colors, 0);

    for (edge_id_t i = 0; i < n_sys_edges; i++) {
        const color_t c = arena.getSysEdge(i).color;
        color_count[c]++;
    }

    color_map.resize(n_colors);

    color_t cur_color = 0;
    for (color_t c = 0; c < n_colors; c++) {
        if (color_count[c] != 0) {
            if ((c % 2) != (cur_color % 2)) {
                cur_color++;
            }
            color_map[c] = cur_color;
        }
    }

    n_colors = cur_color + 1;
}

void PGSolver::copy_colors() {
    n_colors = arena.n_colors;
    color_map.resize(n_colors);
    for (color_t c = 0; c < n_colors; c++) {
        color_map[c] = c;
    }
}

void PGSolver::solve() {
    // set number of threads for parallel solving
    int max_threads;
    if (num_threads > 0) {
        max_threads = num_threads;
    }
    else {
        max_threads = omp_get_max_threads();
    }
    if (onthefly_construction && max_threads > 1) {
        // leave one thread for construction of arena
        max_threads--;
    }
    omp_set_num_threads(max_threads);
    parallel = max_threads > 1;

    if (onthefly_construction) {
        while (!arena.solved) {
            {
                std::unique_lock<std::mutex> lock(arena.size_mutex);
                while (!arena.complete && arena.n_env_nodes == n_env_nodes) {
                    arena.change.wait(lock);
                }

                init_solver();
            }

            std::unique_lock<std::mutex> lock(arena.arena_mutex);
            preprocess_and_solve_game();

            if (winner != UNKNOWN) {
                arena.solved = true;
            }
        }
    }
    else {
        init_solver();
        preprocess_and_solve_game();
    }
}

void PGSolver::print_debug(const std::string& str) const {
    if (verbosity >= 5) {
        std::cout << str << std::endl;
    }
}

Player PGSolver::getWinner() const {
    return winner;
}

static bool fun_add_product_labels(PGArena& arena, std::vector<node_id_t>& state_map, mealy::MealyMachine& m, node_id_t n_env_nodes, node_id_t top_state) {
    std::vector<int> accumulated_bits;
    int state_label_bits = arena.computeStateLabels(state_map, accumulated_bits);
    if (state_label_bits < 0) {
        return false;
    }
    else {
        std::vector<SpecSeq<node_id_t>> labels(m.numberOfStates());
        for (node_id_t i = 0; i < n_env_nodes; i++) {
            if (state_map[i] != NODE_NONE) {
                labels[state_map[i]] = arena.getStateLabel(i);
            }
        }
        if (top_state != mealy::NONE_STATE) {
            labels[top_state] = true_clause<node_id_t>(state_label_bits);
        }
        m.setStateLabels(std::move(labels), state_label_bits, std::move(accumulated_bits));

        return true;
    }
}

bool PGSolver::constructMooreMachine(mealy::MealyMachine& m, const bool add_product_labels) const {
    mealy::machine_t machine;

    const SpecSeq<letter_t> any_input = true_clause<letter_t>(arena.n_inputs);
    const SpecSeq<letter_t> any_output = true_clause<letter_t>(arena.n_outputs);
    std::vector<node_id_t> state_map(n_env_nodes, NODE_NONE);

    std::deque<node_id_t> queue;
    queue.push_back(arena.initial_node);
    machine.push_back({});

    state_map[arena.initial_node] = 0;

    while (!queue.empty()) {
        node_id_t env_node = queue.front();
        queue.pop_front();

        mealy::state_id_t state = state_map[env_node];

        edge_id_t env_edge = env_successors[env_node];
        assert(env_edge < arena.n_env_edges);

        BDD input_bdd = arena.getEnvInput(env_edge);
        std::vector<SpecSeq<letter_t>> inputs;
        for (BDDtoSpecSeqIterator<letter_t> in_it(input_bdd, arena.n_inputs); !in_it.done(); in_it++) {
            // add constant inputs
            SpecSeq<letter_t> input_letter = arena.addUnrealizableInputMask(*in_it);
            inputs.push_back(input_letter);
        }

        const node_id_t sys_node = arena.getEnvEdge(env_edge);

        std::map< mealy::Successor, BDD> output_list;
        BDD covered_outputs = arena.noOutput();

        for (edge_id_t sys_edge = arena.getSysSuccsBegin(sys_node); sys_edge != arena.getSysSuccsEnd(sys_node); sys_edge++) {
            const Edge edge = arena.getSysEdge(sys_edge);
            BDD output_bdd = arena.getSysOutput(sys_edge);
            const node_id_t successor = edge.successor;

            assert(successor < arena.n_env_nodes);

            mealy::state_id_t mealy_successor;
            if (state_map[successor] == NODE_NONE) {
                mealy::state_id_t successor_state = machine.size();
                machine.push_back({});
                state_map[successor] = successor_state;
                queue.push_back(successor);
            }
            mealy_successor = state_map[successor];

            mealy::Successor mealy_combined_successor(mealy_successor, inputs);
            auto const result = output_list.insert({ mealy_combined_successor, output_bdd });
            if (!result.second) {
                result.first->second |= output_bdd;
            }
            covered_outputs |= output_bdd;
        }

        if (covered_outputs != arena.anyOutput() && inputs[0] != any_input) {
            // system goes to bottom sometimes, need to add top state for environment if input still relevant
            mealy::Successor top_successor(mealy::TOP_STATE, inputs);
            output_list.insert({ top_successor, !covered_outputs });
        }

        for (const auto& entry : output_list) {
            for (BDDtoSpecSeqIterator<letter_t> out_it(entry.second, arena.n_outputs); !out_it.done(); out_it++) {
                machine[state].push_back( mealy::Transition(entry.first.successor, *out_it, entry.first.output) );
            }
        }
    }

    bool has_top_state = false;
    node_id_t top_state = machine.size();
    for (mealy::state_id_t s = 0; s < machine.size(); s++) {
        for (auto& t : machine[s]) {
            if (t.nextState == mealy::TOP_STATE) {
                has_top_state = true;
                t.nextState = top_state;
            }
        }
    }
    if (has_top_state) {
        SpecSeq<letter_t> top_input = arena.addUnrealizableInputMask(any_input);
        machine.push_back({ mealy::Transition(top_state, any_output, {top_input}) });
    }
    else {
        top_state = mealy::NONE_STATE;
    }

    m.setMachine(std::move(machine));

    if (add_product_labels) {
        return fun_add_product_labels(arena, state_map, m, n_env_nodes, top_state);
    }
    else {
        return true;
    }
}

bool PGSolver::constructMealyMachine(mealy::MealyMachine& m, const bool add_product_labels) const {
    mealy::machine_t machine;

    const SpecSeq<letter_t> any_input = true_clause<letter_t>(arena.n_inputs);
    const SpecSeq<letter_t> any_output = true_clause<letter_t>(arena.n_outputs);
    std::vector<node_id_t> state_map(n_env_nodes, NODE_NONE);

    std::deque<node_id_t> queue;
    queue.push_back(arena.initial_node);
    machine.push_back({});

    state_map[arena.initial_node] = 0;

    while (!queue.empty()) {
        node_id_t env_node = queue.front();
        queue.pop_front();

        mealy::state_id_t state = state_map[env_node];

        std::map< mealy::Successor, BDD> input_list;

        for (edge_id_t env_edge = arena.getEnvSuccsBegin(env_node); env_edge != arena.getEnvSuccsEnd(env_node); env_edge++) {
            const node_id_t sys_node = arena.getEnvEdge(env_edge);
            std::map<node_id_t, BDD> successor_list;

            for (edge_id_t sys_edge = arena.getSysSuccsBegin(sys_node); sys_edge != arena.getSysSuccsEnd(sys_node); sys_edge++) {
                if (sys_successors[sys_edge]) {
                    const Edge edge = arena.getSysEdge(sys_edge);
                    BDD output_bdd = arena.getSysOutput(sys_edge);

                    auto const result = successor_list.insert({ edge.successor, output_bdd });
                    if (!result.second) {
                        result.first->second |= output_bdd;
                    }
                }
            }

            // Heuristic: if possible, choose successor that was already explored,
            // and among those, choose successor with with maximum non-determinism

            auto succ_it = successor_list.begin();
            bool succ_explored = false;
            size_t succ_num_outputs = 0;

            for (auto it = successor_list.begin(); it != successor_list.end(); it++) {
                const node_id_t cur_successor = it->first;
                if (cur_successor == NODE_TOP) {
                    // always choose top node
                    succ_it = it;
                    break;
                }
                else {
                    bool cur_succ_explored = (state_map[cur_successor] != NODE_NONE);
                    size_t cur_succ_num_outputs = it->second.CountMinterm(arena.n_outputs);
                    if (
                            (cur_succ_explored && !succ_explored) ||
                            (cur_succ_explored == succ_explored && cur_succ_num_outputs > succ_num_outputs)
                    ) {
                        succ_it = it;
                        succ_num_outputs = cur_succ_num_outputs;
                        succ_explored = cur_succ_explored;
                    }
                }
            }

            const node_id_t successor = succ_it->first;

            mealy::state_id_t mealy_successor;
            if (successor == NODE_TOP) {
                mealy_successor = mealy::TOP_STATE;
            }
            else {
                if (state_map[successor] == NODE_NONE) {
                    mealy::state_id_t successor_state = machine.size();
                    machine.push_back({});
                    state_map[successor] = successor_state;
                    queue.push_back(successor);
                }
                mealy_successor = state_map[successor];
            }

            // add outputs to state
            BDD output_bdd = succ_it->second;
            std::vector<SpecSeq<letter_t>> outputs;
            for (BDDtoSpecSeqIterator<letter_t> out_it(output_bdd, arena.n_outputs); !out_it.done(); out_it++) {
                // add constant outputs
                SpecSeq<letter_t> output_letter = arena.addRealizableOutputMask(*out_it);
                outputs.push_back(output_letter);
            }
            // sort outputs by number of unspecified bits and number of zeros
            if (outputs.size() > 1) {
                std::sort(std::begin(outputs), std::end(outputs),
                      [] (const auto& lhs, const auto& rhs) {
                        int lhs_c1 = popcount(lhs.number);
                        int rhs_c1 = popcount(rhs.number);
                        int lhs_c2 = popcount(lhs.unspecifiedBits);
                        int rhs_c2 = popcount(rhs.unspecifiedBits);
                        return lhs_c1 < rhs_c1 ||
                              (lhs_c1 == rhs_c1 && (lhs_c2 > rhs_c2 ||
                              (lhs_c2 == rhs_c2 && lhs.number < rhs.number)));
                });
            }

            if (mealy_successor != mealy::TOP_STATE || outputs[0] != any_output) {
                // add to bdd
                BDD input_bdd = arena.getEnvInput(env_edge);

                mealy::Successor mealy_combined_successor(mealy_successor, outputs);
                auto const result = input_list.insert({ mealy_combined_successor, input_bdd });
                if (!result.second) {
                    result.first->second |= input_bdd;
                }
            }
        }

        for (const auto& entry : input_list) {
            for (BDDtoSpecSeqIterator<letter_t> in_it(entry.second, arena.n_inputs); !in_it.done(); in_it++) {
                machine[state].push_back( mealy::Transition(entry.first.successor, *in_it, entry.first.output) );
            }
        }
    }

    bool has_top_state = false;
    node_id_t top_state = machine.size();
    for (mealy::state_id_t s = 0; s < machine.size(); s++) {
        for (auto& t : machine[s]) {
            if (t.nextState == mealy::TOP_STATE) {
                has_top_state = true;
                t.nextState = top_state;
            }
        }
    }
    if (has_top_state) {
        SpecSeq<letter_t> top_output = arena.addRealizableOutputMask(any_output);
        machine.push_back({ mealy::Transition(top_state, any_input, {top_output}) });
    }
    else {
        top_state = mealy::NONE_STATE;
    }

    m.setMachine(std::move(machine));

    if (add_product_labels) {
        return fun_add_product_labels(arena, state_map, m, n_env_nodes, top_state);
    }
    else {
        return false;
    }
}

}
