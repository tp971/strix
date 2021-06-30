#pragma once

#include <cstdint>
#include <vector>
#include <limits>
#include <iostream>
#include <tuple>

#include <boost/functional/hash.hpp>

typedef uint64_t letter_t;
constexpr letter_t MIN_LETTER = std::numeric_limits<letter_t>::min();
constexpr letter_t MAX_LETTER = std::numeric_limits<letter_t>::max();

enum Player : int8_t {
    SYS_PLAYER = 1,
    ENV_PLAYER = -1,
    UNKNOWN = 0,
};

enum Parity { EVEN = 0, ODD = 1 };

typedef uint32_t node_id_t;
typedef uint32_t edge_id_t;
typedef uint32_t color_t;

static_assert(sizeof(node_id_t) == sizeof(color_t), "sizes of node type and color type do not match");

constexpr node_id_t NODE_BOTTOM = std::numeric_limits<node_id_t>::max() - 1;
constexpr node_id_t NODE_TOP = std::numeric_limits<node_id_t>::max() - 2;
constexpr node_id_t NODE_NONE = std::numeric_limits<node_id_t>::max() - 3;
constexpr node_id_t NODE_NONE_BOTTOM = std::numeric_limits<node_id_t>::max() - 4;
constexpr node_id_t NODE_NONE_TOP = std::numeric_limits<node_id_t>::max() - 5;

constexpr edge_id_t EDGE_BOTTOM = std::numeric_limits<edge_id_t>::max();

typedef std::vector<node_id_t> product_state_t;

struct Edge {
    node_id_t successor;
    color_t color;

    Edge() : successor(NODE_NONE), color(0) {};
    Edge(const node_id_t successor, const color_t color) : successor(successor), color(color) {};

    bool operator==(const Edge& other) const {
        return successor == other.successor && color == other.color;
    }
    bool operator!=(const Edge& other) const {
        return !(operator==(other));
    }
    bool operator<(const Edge& other) const {
        return std::tie(successor, color) < std::tie(other.successor, other.color);
    }
};

template <>
struct std::hash<Edge> {
    std::size_t operator()(const Edge& edge) const {
        std::size_t seed = 0;
        boost::hash_combine(seed, edge.successor);
        boost::hash_combine(seed, edge.color);
        return seed;
    }
};

struct ColorScore {
    color_t color;
    double score;
    double weight;

    ColorScore() :
        color(0), score(0.0), weight(1.0)
    {};
    ColorScore(const color_t color, const double score, const double weight) :
        color(color), score(score), weight(weight)
    {};
};

struct ScoredEdge {
    node_id_t successor;
    ColorScore cs;

    ScoredEdge() :
        successor(NODE_NONE), cs()
    {};
    ScoredEdge(const node_id_t successor, const color_t color, const double score, const double weight) :
        successor(successor), cs(ColorScore(color, score, weight))
    {};
};

enum class ExplorationStrategy {
    BFS,
    PQ
};

std::ostream& operator<<(std::ostream& out, const ExplorationStrategy& exploration);
std::istream& operator>>(std::istream& in, ExplorationStrategy& exploration);
