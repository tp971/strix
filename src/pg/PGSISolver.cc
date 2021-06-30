#include "PGSISolver.h"

#include <iomanip>

namespace pg {

inline distance_t PGSISolver::color_distance_delta(const color_t& color) {
    return 1 - (((arena.parity_type + color) & 1) << 1);
}

PGSISolver::PGSISolver(pg::PGArena& arena, const bool onthefly_construction, const int num_threads, const bool compact_colors, const int verbosity) :
    PGSolver(arena, onthefly_construction, num_threads, compact_colors, verbosity)
{ }

PGSISolver::~PGSISolver() { }

template <Player P>
void PGSISolver::strategy_iteration() {
    print_values_debug();

    bool change = true;
    while (change && winner == UNKNOWN) {
        bellman_ford<P>();
        print_debug("Executing strategy improvement…");
        change = strategy_improvement<P>();
        print_values_debug();
        print_debug("Marking solved nodes");
        update_nodes<P>();
        print_values_debug();
    }
}

template <Player P>
void PGSISolver::update_nodes() {
    #pragma omp parallel for if (parallel)
    for (node_id_t i = 0; i < n_env_nodes; i++) {
        if (arena.getEnvWinner(i) == UNKNOWN && env_distances[i * n_colors] == P*DISTANCE_INFINITY) {
            // node won by current player
            arena.setEnvWinner(i, P);
        }
    }
    #pragma omp parallel for if (parallel)
    for (node_id_t i = 0; i < n_sys_nodes; i++) {
        if (arena.getSysWinner(i) == UNKNOWN && sys_distances[i * n_colors] == P*DISTANCE_INFINITY) {
            // node won by current player
            arena.setSysWinner(i, P);
            if constexpr(P == SYS_PLAYER) {
                // need to deactivate non-winning edges for non-deterministic strategy
                for (edge_id_t j = arena.getSysSuccsBegin(i); j != arena.getSysSuccsEnd(i); j++) {
                    if (sys_successors[j]) {
                        const Edge edge = arena.getSysEdge(j);
                        if (
                                edge.successor < n_env_nodes &&
                                arena.getEnvWinner(edge.successor) == UNKNOWN &&
                                env_distances[edge.successor * n_colors] < DISTANCE_INFINITY
                        ) {
                            sys_successors[j] = false;
                        }
                    }
                }
            }
        }
    }
    winner = arena.getEnvWinner(arena.initial_node);
}

template <Player P>
void PGSISolver::bellman_ford() {
    print_debug("Executing Bellman-Ford algorithm…");
    bellman_ford_init<P>();
    print_values_debug();
    bool change = true;
    while (change) {
        print_debug("Executing Bellman-Ford iteration…");
        if constexpr(P == SYS_PLAYER) {
            bellman_ford_sys_iteration<P>();
        }
        else {
            bellman_ford_env_iteration<P>();
        }
        print_values_debug();
        print_debug("Executing Bellman-Ford iteration…");
        if constexpr(P == SYS_PLAYER) {
            change = bellman_ford_env_iteration<P>();
        }
        else {
            change = bellman_ford_sys_iteration<P>();
        }
        print_values_debug();
    }
}

template <Player P>
void PGSISolver::bellman_ford_init() {
    #pragma omp parallel for if (parallel)
    for (node_id_t i = 0; i < n_sys_nodes; i++) {
        if (arena.getSysWinner(i) == P || (P == ENV_PLAYER && arena.getSysWinner(i) == UNKNOWN)) {
            sys_distances[i * n_colors] = P*DISTANCE_INFINITY;
        }
        else {
            const dist_id_t k = i * n_colors;
            for (dist_id_t l = k; l < k + n_colors; l++) {
                sys_distances[l] = 0;
            }
        }
    }
    #pragma omp parallel for if (parallel)
    for (node_id_t i = 0; i < n_env_nodes; i++) {
        if (arena.getEnvWinner(i) == P || (P == SYS_PLAYER && arena.getEnvWinner(i) == UNKNOWN)) {
            env_distances[i * n_colors] = P*DISTANCE_INFINITY;
        }
        else {
            const dist_id_t k = i * n_colors;
            for (dist_id_t l = k; l < k + n_colors; l++) {
                env_distances[l] = 0;
            }
        }
    }
}

template <Player P>
bool PGSISolver::bellman_ford_sys_iteration() {
    bool change = false;
    #pragma omp parallel for if (parallel)
    for (node_id_t i = 0; i < n_sys_nodes; i++) {
        if (arena.getSysWinner(i) == UNKNOWN) {
            const dist_id_t k = i * n_colors;
            if constexpr(P == SYS_PLAYER) {
                // need to compare against 0 for non-deterministic strategies
                for (dist_id_t l = k; l < k + n_colors; l++) {
                    sys_distances[l] = 0;
                }
            }
            for (edge_id_t j = arena.getSysSuccsBegin(i); j != arena.getSysSuccsEnd(i); j++) {
                if (P == ENV_PLAYER || sys_successors[j]) {
                    const Edge edge = arena.getSysEdge(j);
                    dist_id_t m = edge.successor * n_colors;

                    if (edge.successor == NODE_BOTTOM) {
                        continue;
                    }
                    else if (edge.successor == NODE_TOP) {
                        if (sys_distances[k] != DISTANCE_INFINITY) {
                            change = true;
                            sys_distances[k] = DISTANCE_INFINITY;
                        }
                        break;
                    }
                    else if (edge.successor < n_env_nodes) {
                        if (env_distances[m] == DISTANCE_INFINITY) {
                            if (sys_distances[k] != DISTANCE_INFINITY) {
                                change = true;
                                sys_distances[k] = DISTANCE_INFINITY;
                            }
                            break;
                        }
                        else if (env_distances[m] == DISTANCE_MINUS_INFINITY) {
                            // skip successor
                            continue;
                        }
                    }
                    // successor distance is finite, may not yet be explored
                    bool local_change = false;

                    const color_t cur_color = color_map[edge.color];
                    const distance_t cur_color_change = color_distance_delta(cur_color);
                    sys_distances[k + cur_color] -= cur_color_change;

                    for (dist_id_t l = k; l < k + n_colors; l++, m++) {
                        const distance_t d = sys_distances[l];
                        distance_t d_succ;
                        if (edge.successor < n_env_nodes) {
                            d_succ = env_distances[m];
                        }
                        else {
                            d_succ = 0;
                        }
                        if (local_change || d_succ > d) {
                            sys_distances[l] = d_succ;
                            local_change = true;
                        }
                        else if (d_succ != d) {
                            break;
                        }
                    }
                    sys_distances[k + cur_color] += cur_color_change;

                    if (local_change) {
                        change = true;
                    }
                }
            }
        }
    }
    return change;
}

template <Player P>
bool PGSISolver::bellman_ford_env_iteration() {
    bool change = false;
    #pragma omp parallel for if (parallel)
    for (node_id_t i = 0; i < n_env_nodes; i++) {
        if (arena.getEnvWinner(i) == UNKNOWN) {
            if constexpr(P == SYS_PLAYER) {
                for (edge_id_t j = arena.getEnvSuccsBegin(i); j != arena.getEnvSuccsEnd(i); j++) {
                    const node_id_t successor = arena.getEnvEdge(j);
                    dist_id_t m = successor * n_colors;

                    if (sys_distances[m] < DISTANCE_INFINITY) {
                        bool local_change = false;

                        const dist_id_t k = i * n_colors;
                        for (dist_id_t l = k; l < k + n_colors; l++, m++) {
                            const distance_t d = env_distances[l];
                            const distance_t d_succ = sys_distances[m];
                            if (local_change || d_succ < d) {
                                env_distances[l] = d_succ;
                                local_change = true;
                            }
                            else if (d_succ != d) {
                                break;
                            }
                        }
                        if (local_change) {
                            change = true;
                        }
                    }
                }
            }
            else {
                const edge_id_t j = env_successors[i];
                if (j != EDGE_BOTTOM) {
                    const edge_id_t successor = arena.getEnvEdge(j);
                    dist_id_t m = successor * n_colors;
                    const dist_id_t k = i * n_colors;
                    for (dist_id_t l = k; l < k + n_colors; l++, m++) {
                        env_distances[l] = sys_distances[m];
                    }
                }
            }
        }
    }
    return change;
}

template <>
bool PGSISolver::strategy_improvement<SYS_PLAYER>() {
    bool change = false;
    #pragma omp parallel for if (parallel)
    for (node_id_t i = 0; i < n_sys_nodes; i++) {
        const dist_id_t k = i * n_colors;
        if (arena.getSysWinner(i) == UNKNOWN && sys_distances[k] < DISTANCE_INFINITY) {
            for (edge_id_t j = arena.getSysSuccsBegin(i); j != arena.getSysSuccsEnd(i); j++) {
                sys_successors[j] = false;
                const Edge edge = arena.getSysEdge(j);

                if (edge.successor == NODE_TOP) {
                    sys_successors[j] = true;
                    change = true;
                }
                else if (edge.successor < n_env_nodes && arena.getEnvWinner(edge.successor) != ENV_PLAYER) {
                    bool improvement = true;
                    dist_id_t m = edge.successor * n_colors;

                    const color_t cur_color = color_map[edge.color];
                    const distance_t cur_color_change = color_distance_delta(cur_color);
                    sys_distances[k + cur_color] -= cur_color_change;

                    for (dist_id_t l = k; l < k + n_colors; l++, m++) {
                        const distance_t d = sys_distances[l];
                        const distance_t d_succ = env_distances[m];
                        if (d_succ > d) {
                            // strict improvement
                            change = true;
                            break;
                        }
                        else if (d_succ != d) {
                            improvement = false;
                            break;
                        }
                    }

                    sys_distances[k + cur_color] += cur_color_change;

                    if (improvement) {
                        sys_successors[j] = true;
                    }
                }
            }
        }
    }
    return change;
}

template <>
bool PGSISolver::strategy_improvement<ENV_PLAYER>() {
    bool change = false;
    #pragma omp parallel for if (parallel)
    for (node_id_t i = 0; i < n_env_nodes; i++) {
        const dist_id_t k = i * n_colors;
        if (arena.getEnvWinner(i) == UNKNOWN && env_distances[k] > DISTANCE_MINUS_INFINITY) {
            for (edge_id_t j = arena.getEnvSuccsBegin(i); j != arena.getEnvSuccsEnd(i); j++) {
                const node_id_t successor = arena.getEnvEdge(j);
                if (arena.getSysWinner(successor) != SYS_PLAYER) {
                    bool improvement = false;
                    dist_id_t m = successor * n_colors;
                    if (sys_distances[m] == DISTANCE_MINUS_INFINITY) {
                        improvement = true;
                    }
                    else {
                        for (dist_id_t l = k; l < k + n_colors; l++, m++) {
                            const distance_t d = env_distances[l];
                            const distance_t d_succ = sys_distances[m];
                            if (d_succ < d) {
                                // strict improvement
                                improvement = true;
                                break;
                            }
                            else if (d_succ != d) {
                                break;
                            }
                        }
                    }

                    if (improvement) {
                        change = true;
                        env_successors[i] = j;
                        break;
                    }
                }
            }
        }
    }
    return change;
}

inline void PGSISolver::print_values_debug() {
    if (verbosity >= 6) {
        std::cout << std::setfill('-') << std::setw(20) << "" << std::setfill(' ') << std::endl;
        std::cout << "---- sys nodes -----" << std::endl;
        std::cout << std::setfill('-') << std::setw(20) << "" << std::setfill(' ') << std::endl;
        std::cout << "        v     o(v)    d(v)" << std::endl;
        for (size_t i = 0; i < n_sys_nodes; i++) {
            if (i == arena.initial_node) {
                std::cout << ">";
            }
            else {
                std::cout << " ";
            }
            std::cout << std::setw(8) << i;

            std::cout << " [";
            for (edge_id_t j = arena.getSysSuccsBegin(i); j != arena.getSysSuccsEnd(i); j++) {
                if (sys_successors[j]) {
                    node_id_t s = arena.getSysEdge(j).successor;
                    if (s == NODE_TOP) {
                        std::cout << "  ⊤";
                    }
                    else if (s == NODE_BOTTOM) {
                        std::cout << "  ⊥";
                    }
                    else if (s == NODE_NONE) {
                        std::cout << "  -";
                    }
                    else {
                        std:: cout << " " << std::setw(2) << s;
                    }
                }
            }
            std::cout << " ]";

            for (color_t c = 0; c < n_colors; c++) {
                distance_t distance = sys_distances[i*n_colors + c];
                if (distance == DISTANCE_INFINITY) {
                    std::cout << "       ∞";
                    break;
                }
                else if (distance == DISTANCE_MINUS_INFINITY) {
                    std::cout << "      -∞";
                    break;
                }
                else {
                    std::cout << std::setw(8) << distance;
                }
            }
            if (arena.getSysWinner(i) == SYS_PLAYER) {
                std::cout << "  won_sys";
            }
            else if (arena.getSysWinner(i) == ENV_PLAYER) {
                std::cout << "  won_env";
            }
            std::cout << std::endl;
        }
        std::cout << std::setfill('-') << std::setw(20) << "" << std::setfill(' ') << std::endl;
        std::cout << "---- env nodes -----" << std::endl;
        std::cout << std::setfill('-') << std::setw(20) << "" << std::setfill(' ') << std::endl;
        std::cout << "        v     d(v)" << std::endl;
        for (size_t i = 0; i < n_env_nodes; i++) {
            if (i == arena.initial_node) {
                std::cout << ">";
            }
            else {
                std::cout << " ";
            }
            std::cout << std::setw(8) << i;

            std::cout << " [";
            if (env_successors[i] == EDGE_BOTTOM) {
                std::cout << "⊥";
            }
            else {
                std::cout << arena.getEnvEdge(env_successors[i]);
            }
            std::cout << "]";

            for (color_t c = 0; c < n_colors; c++) {
                distance_t distance = env_distances[i*n_colors + c];
                if (distance == DISTANCE_INFINITY) {
                    std::cout << "       ∞";
                    break;
                }
                else if (distance == DISTANCE_MINUS_INFINITY) {
                    std::cout << "      -∞";
                    break;
                }
                else {
                    std::cout << std::setw(8) << distance;
                }
            }
            if (arena.getEnvWinner(i) == SYS_PLAYER) {
                std::cout << "  won_sys";
            }
            else if (arena.getEnvWinner(i) == ENV_PLAYER) {
                std::cout << "  won_env";
            }
            std::cout << std::endl;
        }
        std::cout << std::setfill('-') << std::setw(20) << "" << std::setfill(' ') << std::endl;
    }
}

void PGSISolver::solve_game() {
    sys_distances = std::vector<distance_t>(n_sys_nodes * n_colors, 0);
    env_distances = std::vector<distance_t>(n_env_nodes * n_colors, 0);

    sys_successors.resize(n_sys_edges, false);
    env_successors.resize(n_env_nodes, EDGE_BOTTOM);

    print_debug("Starting strategy iteration for sys player…");
    strategy_iteration<SYS_PLAYER>();
    print_debug("Starting strategy iteration for env player…");
    strategy_iteration<ENV_PLAYER>();

    // clear memory
    std::vector<distance_t>().swap(sys_distances);
    std::vector<distance_t>().swap(env_distances);
}

}
