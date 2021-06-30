#pragma once

#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include "Definitions.h"

#include "owl.h"

namespace po = boost::program_options;

namespace strix {

struct Options {
    // input options
    std::vector<std::string> input_files;
    std::string formula;
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;

    // output options
    std::string output_file;
    bool mealy;
    bool binary;
    bool dot;
    bool bdd;
    bool parity_game;

    // synthesis options
    bool realizability;
    bool labels;
    bool minimize;
    bool auto_aiger;
    ExplorationStrategy exploration;
    bool clear_queue;
    std::string finite;

    // fine-tuning options
    bool monolithic;
    bool onthefly;
    bool simplify_formula;
    int threads;
    bool compact_colors;
    bool compress_circuit;
    bool validate_jni;
    bool aggressive_heap_optimizations;
    int max_heap_size_gb;
    int initial_heap_size_gb;

    // debug output options
    int verbosity;

    // misc options
    boost::filesystem::path owl_jar;
    bool timing;
    bool help;
};


class OptionParser {
private:
    const int argc;
    const char** argv;

    po::options_description input_options;
    po::options_description output_options;
    po::options_description synthesis_options;
    po::options_description finetuning_options;
    po::options_description debugoutput_options;
    po::options_description misc_options;

    po::options_description hidden_options;
    po::positional_options_description positional_options;
    po::options_description options;

public:
    OptionParser(const int argc, const char* argv[]);
    ~OptionParser();

    void print_help() const;
    Options parse_options() const;
};

}
