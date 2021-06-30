#pragma once

#include "pg/PGArena.h"
#include "pg/PGSolver.h"

namespace pg {

typedef int32_t distance_t;
typedef size_t dist_id_t;
constexpr distance_t DISTANCE_INFINITY = std::numeric_limits<distance_t>::max() - 1;
constexpr distance_t DISTANCE_MINUS_INFINITY = -DISTANCE_INFINITY;
static_assert(DISTANCE_INFINITY > 0, "plus infinity not positive");
static_assert(DISTANCE_INFINITY + 1 > 0, "plus infinity too large");
static_assert(DISTANCE_MINUS_INFINITY < 0, "minus infinity not negative");
static_assert(DISTANCE_MINUS_INFINITY - 1 < 0, "minus infinity too small");

class PGSISolver : public PGSolver {
private:
    std::vector<distance_t> sys_distances;
    std::vector<distance_t> env_distances;

    inline distance_t color_distance_delta(const color_t& color);

    template <Player P>
    void update_nodes();
    template <Player P>
    void bellman_ford();
    template <Player P>
    void bellman_ford_init();
    template <Player P>
    bool bellman_ford_sys_iteration();
    template <Player P>
    bool bellman_ford_env_iteration();
    template <Player P>
    bool strategy_improvement();
    template <Player P>
    void strategy_iteration();

    inline void print_values_debug();

protected:
    void solve_game();

public:
    PGSISolver(pg::PGArena& arena, const bool onthefly_construction, const int num_threads, const bool compact_colors = true, const int verbositiy = 0);
    ~PGSISolver();
};

}
