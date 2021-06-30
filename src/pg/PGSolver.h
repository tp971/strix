#pragma once

#include "Definitions.h"
#include "mealy/MealyMachine.h"
#include "pg/PGArena.h"

namespace pg {

class PGSolver {
private:
    void init_solver();
    void preprocess_and_solve_game();
    void reduce_colors();
    void copy_colors();

protected:
    pg::PGArena& arena;
    const bool onthefly_construction;
    const int num_threads;
    const bool compact_colors;
    const int verbosity;

    bool parallel;

    std::vector<uint8_t> sys_successors;
    std::vector<edge_id_t> env_successors;

    Player winner;

    node_id_t n_env_nodes;
    node_id_t n_env_edges;
    node_id_t n_sys_nodes;
    node_id_t n_sys_edges;

    color_t n_colors;
    std::vector<color_t> color_map;

    virtual void solve_game() = 0;

public:
    PGSolver(pg::PGArena& arena, const bool onthefly_construction, const int num_threads, const bool compact_colors = true, const int verbosity = 0);
    virtual ~PGSolver();

    virtual void solve();

    Player getWinner() const;
    bool constructMooreMachine(mealy::MealyMachine& m, const bool add_product_labels) const;
    bool constructMealyMachine(mealy::MealyMachine& m, const bool add_product_labels) const;

    void print_debug(const std::string& str) const;
};

}
