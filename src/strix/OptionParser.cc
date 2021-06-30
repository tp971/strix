#include "OptionParser.h"

#include <iostream>
#include <boost/algorithm/string/predicate.hpp>

std::ostream& operator<<(std::ostream& out, const ExplorationStrategy& exploration) {
    switch (exploration) {
        case ExplorationStrategy::BFS:
            out << "bfs";
            break;
        case ExplorationStrategy::PQ:
            out << "pq";
            break;
    }
    return out;
}

std::istream& operator>>(std::istream& in, ExplorationStrategy& exploration) {
    std::string token;
    in >> token;
    if (token == "bfs") {
        exploration = ExplorationStrategy::BFS;
    }
    else if (token == "pq") {
        exploration = ExplorationStrategy::PQ;
    }
    else {
        in.setstate(std::ios_base::failbit);
    }
    return in;
}

namespace strix {

struct counter {
    int count = 0;
    friend std::ostream& operator<< (std::ostream& stream, const counter& counter) {
        stream << counter.count;
        return stream;
    }
};
void validate(boost::any& v, std::vector<std::string> const& xs, counter*, long) {
    if (v.empty()) {
        v = counter{1};
    }
    else ++boost::any_cast<counter&>(v).count;
}

OptionParser::OptionParser(const int argc, const char* argv[]) :
    argc(argc),
    argv(argv),
    input_options("Input options"),
    output_options("Output options"),
    synthesis_options("Synthesis options"),
    finetuning_options("Fine-tuning options"),
    debugoutput_options("Debug output options"),
    misc_options("Miscellaneous options"),
    hidden_options(),
    positional_options(),
    options()
{
    input_options.add_options()
        ("formula,f", po::value<std::string>()->default_value(std::string()), "use the LTL formula given by arg instead of FILE")
        ("ins", po::value<std::string>(), "use this comma-separated list of input propositions")
        ("outs", po::value<std::string>(), "use this comma-separated list of output propositions")
    ;
    output_options.add_options()
        ("kiss,k", "output Mealy machine in KISS format")
        ("binary", "output AIGER circuit in binary format")
        ("dot,d", "output Mealy machine in Graphviz DOT format")
        ("bdd,b", "output BDDs in DOT format")
        ("parity-game,p", "output parity game in PGSolver format")
        ("output,o", po::value<std::string>()->default_value(""), "output controller to this file")
    ;
    synthesis_options.add_options()
        ("realizability,r", "only check realizability")
        ("labels,l", "use product state labels for Mealy machine")
        ("minimize,m", "minimize Mealy machine")
        ("auto,a", "automatically find configuration producing minimal AIGER circuit")
        ("exploration,e", po::value<ExplorationStrategy>()->default_value(ExplorationStrategy::BFS), "exploration strategy (bfs or pq)")
        ("clear-queue,c", "regularly clear exploration queue from unreachable and already winning states")
        ("from-ltlf", po::value<std::string>()->default_value("")->implicit_value("alive"), "transform LTLf (finite LTL) to LTL by introducing some 'alive' proposition")
    ;
    finetuning_options.add_options()
        ("monolithic", "do not use formula splitting")
        ("no-onthefly", "do not construct and solve arena on-the-fly")
        ("no-simplify-formula", "do not simplify the formula")
        ("threads", po::value<int>()->default_value(0, "auto"), "set the number of solver threads")
        ("no-compact-colors", "do not compact the colors of the parity game")
        ("no-compress-circuit", "do not compress the AIGER circuit using ABC")
        ("validate-jni", "validate JNI interface")
        ("optimize-heap", "optimize Java heap size")
        ("max-heap-size", po::value<int>()->default_value(0, "auto"), "maximum heap size in GB")
        ("initial-heap-size", po::value<int>()->default_value(0, "auto"), "initial heap size in GB")
    ;
    debugoutput_options.add_options()
        ("verbose,v", po::value<counter>()
            ->default_value(counter{0})
            ->zero_tokens(),
            "verbose output (may be specified more than once)")
    ;
    misc_options.add_options()
        ("owl-jar", po::value<std::string>(), "jar file for Owl library")
        ("timing,t", "measure and print timing information")
        ("help,h", "display this help text and exit")
    ;
    hidden_options.add_options()
        ("input-file", po::value<std::vector<std::string>>(), "input files")
    ;
    options
        .add(input_options)
        .add(output_options)
        .add(synthesis_options)
        .add(finetuning_options)
        .add(debugoutput_options)
        .add(misc_options)
        .add(hidden_options);
    positional_options.add("input-file", -1);
}

OptionParser::~OptionParser() { }

void OptionParser::print_help() const {
    std::cout << "Usage: " << argv[0] << " [OPTIONS] FILE --ins=INS --outs=OUTS" << std::endl;
    std::cout << "Synthesize controllers according to the LTL formula in FILE," << std::endl;
    std::cout << "where INS are the input propositions and OUTS the output propositions" << std::endl;
    std::cout << "Outputs the controllers in textual AIGER format by default." << std::endl;
    std::cout << std::endl;
    std::cout << input_options << std::endl;
    std::cout << output_options << std::endl;
    std::cout << synthesis_options << std::endl;
    std::cout << finetuning_options << std::endl;
    std::cout << debugoutput_options << std::endl;
    std::cout << misc_options << std::endl;
}

Options OptionParser::parse_options() const {
    po::variables_map vm;

    po::store(po::command_line_parser(argc, argv).
            options(options).
            positional(positional_options).
            run(), vm);
    po::notify(vm);

    Options options;

    // input options
    if (vm.count("input-file") > 0) {
        options.input_files = vm["input-file"].as<std::vector<std::string>>();
    }
    options.formula = vm["formula"].as<std::string>();
    const auto is_sep = boost::is_any_of(",; \t\r\n");
    if (vm.count("ins") > 0) {
        std::string inputs_string = vm["ins"].as<std::string>();
        boost::algorithm::trim_if(inputs_string, is_sep);
        if (!inputs_string.empty()) {
            boost::split(options.inputs, inputs_string, is_sep, boost::token_compress_on);
        }
    }
    if (vm.count("outs") > 0) {
        std::string outputs_string = vm["outs"].as<std::string>();
        boost::algorithm::trim_if(outputs_string, is_sep);
        if (!outputs_string.empty()) {
            boost::split(options.outputs, outputs_string, is_sep, boost::token_compress_on);
        }
    }

    // output options
    options.output_file = vm["output"].as<std::string>();
    options.mealy = vm.count("kiss") > 0;
    options.binary = vm.count("binary") > 0;
    options.dot = vm.count("dot") > 0;
    options.bdd = vm.count("bdd") > 0;
    options.parity_game = vm.count("parity-game") > 0;

    // synthesis options
    options.realizability = vm.count("realizability") > 0;
    options.labels = vm.count("labels") > 0;
    options.minimize = vm.count("minimize") > 0;
    options.auto_aiger = vm.count("auto") > 0;
    if (options.auto_aiger) {
        // need to minimize and produce labels for this option
        options.labels = true;
        options.minimize = true;
    }
    options.exploration = vm["exploration"].as<ExplorationStrategy>();
    options.clear_queue = vm.count("clear-queue") > 0;
    options.finite = vm["from-ltlf"].as<std::string>();

    // fine-tuning options
    options.monolithic = vm.count("monolithic") > 0;
    options.onthefly = vm.count("no-onthefly") == 0;
    options.simplify_formula = vm.count("no-simplify-formula") == 0;
    options.threads = vm["threads"].as<int>();
    if (options.threads < 0) {
        throw std::invalid_argument("Invalid number of threads: " + std::to_string(options.threads));
    }
    options.compact_colors = vm.count("no-compact-colors") == 0;
    options.compress_circuit = vm.count("no-compress-circuit") == 0;
    options.validate_jni = vm.count("validate-jni") > 0;
    options.aggressive_heap_optimizations = vm.count("optimize-heap") > 0;
    options.max_heap_size_gb = vm["max-heap-size"].as<int>();
    if (options.max_heap_size_gb < 0) {
        throw std::invalid_argument("Invalid max heap size: " + std::to_string(options.max_heap_size_gb));
    }
    options.initial_heap_size_gb = vm["initial-heap-size"].as<int>();
    if (options.initial_heap_size_gb < 0) {
        throw std::invalid_argument("Invalid initial heap size: " + std::to_string(options.initial_heap_size_gb));
    }

    // debug output options
    options.verbosity = vm["verbose"].as<counter>().count;

    // misc options
    if (vm.count("owl-jar")) {
        options.owl_jar = boost::filesystem::path(vm["owl-jar"].as<std::string>());
    }
    else {
        // look for "owl.jar" in same directory as binary
        options.owl_jar = boost::filesystem::system_complete(boost::filesystem::path(argv[0])).parent_path() / "owl.jar";
    }
    options.timing = vm.count("timing") > 0;
    options.help = vm.count("help") > 0;

    return options;
}

}
