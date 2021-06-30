#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <memory>
#include <thread>

#include "OptionParser.h"
#include "Definitions.h"
#include "util/Timer.h"
#include "ltl/LTLParser.h"
#include "ltl/Specification.h"
#include "mealy/MealyMachine.h"
#include "pg/PGArena.h"
#include "pg/PGSolver.h"
#include "pg/PGSISolver.h"
#include "aig/AigerConstructor.h"

#include "owl.h"

std::string construct_classpath(const strix::Options& options) {
    if (boost::filesystem::exists(options.owl_jar)) {
        std::stringstream cp;
        cp << "-Djava.class.path=" << "\":" << options.owl_jar.string() << ":\"";
        return cp.str();
    }
    else {
        throw std::invalid_argument("Cannot find owl jar: " + options.owl_jar.string());
    }
}

std::unique_ptr<mealy::MealyMachine> check_realizability(const strix::Options& options, std::ostream& out, owl::OwlThread& owl, const ltl::Specification& spec) {
    Timer realizability_timer(options.timing);
    Timer timer(options.timing);

    realizability_timer.start("checking realizability");

    if (options.verbosity >= 1) {
        std::cout << "Constructing automaton for formula ";
        spec.formula.print();
    }
    timer.start("constructing automaton tree");
    const int firstOutputVariable = spec.inputs.size();
    owl::DecomposedDPA automaton = owl.createAutomaton(spec.formula, options.simplify_formula, options.monolithic, firstOutputVariable);
    aut::AutomatonTreeStructure structure(std::move(automaton));
    pg::PGArena arena(spec.inputs.size(), spec.outputs.size(), structure, options.exploration, options.clear_queue);
    timer.stop();

    if (options.onthefly) {
        timer.start("solving game in parallel with construction of arena");
    }
    else {
        timer.start("constructing arena");
        arena.constructArena(false, options.realizability, options.verbosity);
        timer.stop();
        timer.start("solving game");
    }

    pg::PGSISolver solver(arena, options.onthefly, options.threads, options.compact_colors, options.verbosity);
    if (options.onthefly) {
        std::thread solver_thread = std::thread(&pg::PGSolver::solve, &solver);
        arena.constructArena(true, options.realizability, options.verbosity);
        solver_thread.join();
    }
    else {
        solver.solve();
    }
    timer.stop();

    Player winner = solver.getWinner();
    switch (winner) {
        case SYS_PLAYER:
            std::cout << "REALIZABLE" << std::endl;
            break;
        case ENV_PLAYER:
            std::cout << "UNREALIZABLE" << std::endl;
            break;
        case UNKNOWN:
            std::cout << "UNKNOWN" << std::endl;
            break;
    }
    realizability_timer.stop();

    if (options.verbosity >= 1) {
        arena.print_basic_info();
    }
    if (options.parity_game) {
        timer.start("writing parity game");
        arena.print(out, winner);
        timer.stop();
    }

    std::unique_ptr<mealy::MealyMachine> m;
    if (winner != UNKNOWN && !options.realizability) {
        bool with_labels = true;
        if (winner == SYS_PLAYER) {
            timer.start("constructing Mealy machine");
            m = std::unique_ptr<mealy::MealyMachine>(new mealy::MealyMachine(spec.inputs, spec.outputs, mealy::Semantic::MEALY));
            with_labels = solver.constructMealyMachine(*m, options.labels);
            timer.stop();
        }
        else if (winner == ENV_PLAYER) {
            timer.start("constructing Moore machine");
            m = std::unique_ptr<mealy::MealyMachine>(new mealy::MealyMachine(spec.outputs, spec.inputs, mealy::Semantic::MOORE));
            with_labels = solver.constructMooreMachine(*m, options.labels);
            timer.stop();
        }
        if (options.labels && !m->hasLabels() && options.verbosity >= 1) {
            std::cerr << "Error: cannot construct product labels" << std::endl;
        }
    }
    return m;
}

void construct_solution(const strix::Options& options, std::ostream& out, mealy::MealyMachine& m) {
    Timer solution_timer(options.timing);
    Timer timer(options.timing);
    solution_timer.start("constructing solution");

    if (options.verbosity >= 1) {
        std::cout << "Number of states of Mealy machine: " << m.numberOfStates() << std::endl;
    }
    // in auto mode, do not try to minimize huge Mealy machines
    bool minimize = options.minimize && (!options.auto_aiger || m.numberOfStates() < 10000);
    if (minimize) {
        timer.start("minimize Mealy machine");
        if (options.verbosity >= 1) {
            std::cout << "Minimizing Mealy machine" << std::endl;
        }
        m.minimizeMachine(options.verbosity);
        if (options.verbosity >= 1) {
            if (m.hasMinimized()) {
                std::cout << "Number of states of minimized Mealy machine: " << m.numberOfMinStates() << std::endl;
            }
            else {
                std::cout << "Minimizing Mealy machine failed" << std::endl;
            }
        }
        timer.stop();
    }
    if (options.mealy) {
        if (options.dot) {
            m.print_dot(out);
        }
        else {
            m.print_kiss(out);
        }
    }
    else {
        timer.start("constructing AIGER circuit");
        const bool use_labels = m.hasLabels();
        const bool use_minimized = m.hasMinimized();
        std::shared_ptr<aig::AigerConstructor> aiger;
        if (options.auto_aiger) {
            aiger = aig::AigerConstructor::findMinimalAiger(m, use_labels, use_minimized, options.compress_circuit, options.bdd);
        }
        else {
            aiger = std::shared_ptr<aig::AigerConstructor>(new aig::AigerConstructor(m, use_labels, use_minimized));
            aiger->construct(options.compress_circuit, options.bdd);
        }
        if (options.bdd) {
            aiger->printBDDs(out);
        }
        else if (options.dot) {
            aiger->print_dot(out);
        }
        else {
            aiger->print_aiger(out, options.binary);
        }
        timer.stop();
    }
}

void synthesis(const strix::Options& options) {
    Timer strix_timer(options.timing);
    Timer timer(options.timing);
    strix_timer.start("running Strix");

    std::streambuf* buf;
    std::ofstream of;
    if (options.output_file.empty()) {
        buf = std::cout.rdbuf();
    }
    else {
        of.open(options.output_file, std::ios::out);
        if (!of) {
            throw std::invalid_argument("Could not open output file: " + options.output_file);
        }
        buf = of.rdbuf();
    }
    std::ostream out(buf);

    timer.start("initializing OWL-JNI");
    const std::string classpath_option = construct_classpath(options);
    owl::OwlJavaVM owlJavaVM(classpath_option.c_str(), options.validate_jni, options.initial_heap_size_gb, options.max_heap_size_gb, options.aggressive_heap_optimizations);
    owl::OwlThread owl = owlJavaVM.attachCurrentThread();
    timer.stop();

    timer.start("parsing input files");
    std::vector<ltl::Specification> specifications;
    std::unique_ptr<ltl::Parser> parser(new ltl::LTLParser(owl, options.inputs, options.outputs, options.finite));
    if (options.formula.empty() && options.input_files.empty()) {
        throw std::invalid_argument("No input files or formula was given.");
    }
    if (!options.formula.empty()) {
        specifications.push_back(parser->parse_string(options.formula, options.verbosity));
    }
    for (const auto& input_file : options.input_files) {
        specifications.push_back(parser->parse_string(parser->parse_file(input_file), options.verbosity));
    }
    parser.reset();
    timer.stop();

    for (const auto& spec : specifications) {
        std::unique_ptr<mealy::MealyMachine> m = check_realizability(options, out, owl, spec);
        if (m) {
            construct_solution(options, out, *m);
        }
    }

    of.close();
    strix_timer.stop();
}

int main(const int argc, const char* argv[]) {
    try {
        // Parse options
        const strix::OptionParser option_parser(argc, argv);
        const strix::Options options = option_parser.parse_options();

        if (options.help) {
            option_parser.print_help();
            return EXIT_SUCCESS;
        }
        else {
            synthesis(options);
        }

    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    catch (const std::string& e) {
        std::cerr << "Error: " << e << std::endl;
        return EXIT_FAILURE;
    }
    catch (...) {
        std::cerr << "Error: Unknown error." << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
