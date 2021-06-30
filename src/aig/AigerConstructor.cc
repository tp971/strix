#include "AigerConstructor.h"

#include <cstdio>
#include <cstdlib>
#include <thread>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string.hpp>

namespace aig {

// wrapper to use std::ostream as FILE* pointer
template<class STREAM>
struct STDIOAdapter {
    static FILE* yield(STREAM* stream) {
        assert(stream != NULL);

#ifdef __APPLE__
        return fwopen(static_cast<void*>(stream), cookieWrite);
#else
        static cookie_io_functions_t Cookies;
        Cookies.read = NULL;
        Cookies.write = cookieWrite;
        Cookies.seek = NULL;
        Cookies.close = cookieClose;

        return fopencookie(stream, "w", Cookies);
#endif
    }

#ifdef __APPLE__
    static int cookieWrite(void* cookie, const char* buf, int size) {
#else
    static ssize_t cookieWrite(void* cookie, const char* buf, size_t size) {
#endif
        if(cookie == NULL) {
            return -1;
        }
        STREAM* stream = static_cast<STREAM*>(cookie);
        stream->write(buf, size);
        return size;
    }

    static int cookieClose(void* cookie) {
         return EOF;
    }
};

AigerConstructor::AigerConstructor(const mealy::MealyMachine& m, const bool use_labels, const bool minimize) :
    m(m), use_labels(use_labels), minimize(minimize), aig(aiger_init())
{
    if (use_labels) {
        n_latches = m.getStateLabelBits();
    }
    else {
        if (minimize) {
            n_states = m.numberOfMinStates();
        }
        else {
            n_states = m.numberOfStates();
        }
        n_latches = (size_t)ceil(log2(n_states));
    }
    and_gate_index = m.n_inputs + n_latches;
}

AigerConstructor::~AigerConstructor() {
    aiger_reset(aig);
}

BDD AigerConstructor::constructBDDForVar(var_t var, const transition_vec& machine) {
    BDD bdd = manager.bddZero();
    bool is_latch_var = false;
    if (var >= m.n_outputs) {
        is_latch_var = true;
        var -= m.n_outputs;
    }
    const letter_t bit = ((letter_t)1 << var);
    for (mealy::state_id_t s = 0; s < machine.size(); s++) {
        SpecSeq<node_id_t> cur_state(s);
        if (use_labels) {
            cur_state = m.getStateLabel(s, minimize);
        }
        for (const auto& t : machine[s]) {
            SpecSeq<letter_t> outputBits;
            if (is_latch_var) {
                if (use_labels) {
                    SpecSeq<node_id_t> stateBits = m.getStateLabel(t.nextState, minimize);
                    outputBits = SpecSeq<letter_t>(stateBits.number, stateBits.unspecifiedBits);
                }
                else {
                    outputBits = SpecSeq<letter_t>(t.nextState);
                }
            }
            else {
                // TODO: make better use of non-determinism
                outputBits = t.output[0];
            }
            if (((outputBits.unspecifiedBits & bit) == 0) && ((outputBits.number & bit) != 0)) {
                BDD bdd_input = manager.bddOne();

                if (is_latch_var || m.semantic == mealy::Semantic::MEALY) {
                    // only latches and Mealy outputs may depend on inputs
                    for (var_t input_var = 0; input_var < m.n_inputs; input_var++) {
                        const letter_t input_bit = ((letter_t)1 << input_var);
                        if ((t.input.unspecifiedBits & input_bit) == 0) {
                            BDD bdd_input_var = manager.bddVar(input2bddvar(input_var));
                            if ((t.input.number & input_bit) == 0) {
                                bdd_input &= !bdd_input_var;
                            }
                            else {
                                bdd_input &= bdd_input_var;
                            }
                        }
                    }
                }
                for (var_t latch_var = 0; latch_var < n_latches; latch_var++) {
                    const mealy::state_id_t latch_bit = ((letter_t)1 << latch_var);
                    if ((cur_state.unspecifiedBits & latch_bit) == 0) {
                        BDD bdd_latch_var = manager.bddVar(latch2bddvar(latch_var));
                        if ((cur_state.number & latch_bit) == 0) {
                            bdd_input &= !bdd_latch_var;
                        }
                        else {
                            bdd_input &= bdd_latch_var;
                        }
                    }
                }

                bdd |= bdd_input;
            }
            if (!is_latch_var && m.semantic == mealy::Semantic::MOORE) {
                // only need to look at one transition for output of state
                break;
            }
        }
    }

    // minimize function not fully exposed in C++ API, need to call C API
    //BDD min_bdd(manager, Cudd_bddMinimize(manager.getManager(), bdd.getNode(), care_set.getNode()));

    return bdd;
}

inline literal_t AigerConstructor::add_and(literal_t lhs, literal_t rhs, std::unordered_map<literal_pair_t, literal_t>& aig_cache) {
    if (lhs == rhs) {
        return lhs;
    }
    else if (lhs == aiger_not(rhs) || lhs == aiger_false || rhs == aiger_false) {
        return aiger_false;
    }
    else if (lhs == aiger_true) {
        return rhs;
    }
    else if (rhs == aiger_true) {
        return lhs;
    }
    else if (lhs > rhs) {
        // normalize for cache
        literal_t tmp = lhs;
        lhs = rhs;
        rhs = tmp;
    }
    const literal_pair_t key(lhs, rhs);
    const auto it = aig_cache.find(key);
    if (it != aig_cache.end()) {
        return it->second;
    }
    else {
        and_gate_index++;
        literal_t lit = aiger_var2lit(and_gate_index);
        aiger_add_and(aig, lit, lhs, rhs);
        aig_cache.insert({key, lit});
        return lit;
    }
}

literal_t AigerConstructor::nodeToLiteral(DdNode* node,
        std::unordered_map<DdNode*, literal_t>& bdd_cache,
        std::unordered_map<literal_pair_t, literal_t>& aig_cache
) {
    const var_t var = Cudd_NodeReadIndex(node);
    const bool complement = Cudd_IsComplement(node);
    literal_t lit;
    if (var == CUDD_CONST_INDEX) {
        lit = aiger_true;
    }
    else {
        DdNode* reg_node = Cudd_Regular(node);

        auto it = bdd_cache.find(reg_node);
        if (it != bdd_cache.end()) {
            lit = it->second;
        }
        else {
            DdNode* then_node = Cudd_T(node);
            DdNode* else_node = Cudd_E(node);

            const literal_t then_lit = nodeToLiteral(then_node, bdd_cache, aig_cache);
            const literal_t else_lit = nodeToLiteral(else_node, bdd_cache, aig_cache);

            literal_t var_lit = bddvar2lit(var);

            // special cases not handled by and gate construction
            if (then_lit == aiger_false) {
                lit = add_and(aiger_not(var_lit), else_lit, aig_cache);
            }
            else if (else_lit == aiger_false) {
                lit = add_and(var_lit, then_lit, aig_cache);
            }
            else {
                const literal_t then_or_lit = add_and(var_lit, aiger_not(then_lit), aig_cache);
                const literal_t else_or_lit = add_and(aiger_not(var_lit), aiger_not(else_lit), aig_cache);
                lit = add_and(aiger_not(then_or_lit), aiger_not(else_or_lit), aig_cache);
            }

            bdd_cache.insert({reg_node, lit});
        }
    }

    if (complement) {
        return aiger_not(lit);
    }
    else {
        return lit;
    }
}

void AigerConstructor::printBDDs(std::ostream& out) const {
    char** input_names = new char*[m.n_inputs + n_latches];
    char** output_names = new char*[m.n_outputs + n_latches];
    for (var_t input_var = 0; input_var < m.n_inputs; input_var++) {
        input_names[input_var] = new char[m.inputs[input_var].size() + 1];
        strcpy(input_names[input_var], m.inputs[input_var].c_str());
    }
    for (var_t output_var = 0; output_var < m.n_outputs; output_var++) {
        output_names[output_var] = new char[m.outputs[output_var].size() + 1];
        strcpy(output_names[output_var], m.outputs[output_var].c_str());
    }
    for (var_t latch_var = 0; latch_var < n_latches; latch_var++) {
        input_names[m.n_inputs + latch_var] = new char[100];
        sprintf(input_names[m.n_inputs + latch_var], "l%d", latch_var);

        output_names[m.n_outputs + latch_var] = new char[100];
        sprintf(output_names[m.n_outputs + latch_var], "l%d_in", latch_var);
    }

    FILE* file = STDIOAdapter<std::ostream>::yield(&out);
    if (file == nullptr) {
        throw std::runtime_error("Could not adapt std::ostream as FILE*");
    }
    manager.DumpDot(bdds, input_names, output_names, file);
    fclose(file);

    for (var_t input_var = 0; input_var < m.n_inputs + n_latches; input_var++) {
        delete[] input_names[input_var];
    }
    for (var_t output_var = 0; output_var < m.n_outputs + n_latches; output_var++) {
        delete[] output_names[output_var];
    }
    delete[] input_names;
    delete[] output_names;
}

void AigerConstructor::constructBDDs() {
    bdds.reserve(m.n_outputs + n_latches);
    const transition_vec& machine = m.getTransitions(minimize);
    for (var_t var = 0; var < m.n_outputs + n_latches; var++) {
        BDD bdd = constructBDDForVar(var, machine);
        bdds.push_back(bdd);
    }
}

void AigerConstructor::constructAiger(const bool keepBDDs) {
    const var_t num_vars = m.n_inputs + n_latches;
    manager = Cudd(num_vars);
    manager.AutodynEnable();

    // construct BDDs
    constructBDDs();
    manager.AutodynDisable();

    // reorder nodes
    if (num_vars <= 16) {
        // use exact reordering up to 16 variables
        manager.ReduceHeap(CUDD_REORDER_EXACT);
    }
    else {
        // use sift heuristic until convergence
        manager.ReduceHeap(CUDD_REORDER_SIFT_CONVERGE);
    }

    // construct aiger circuit
    std::unordered_map<DdNode*, literal_t> bdd_cache;
    std::unordered_map<literal_pair_t, literal_t> aig_cache;

    for (var_t input_var = 0; input_var < m.n_inputs; input_var++) {
        aiger_add_input(aig, input2lit(input_var), m.inputs[input_var].c_str());
    }
    for (var_t output_var = 0; output_var < m.n_outputs; output_var++) {
        const literal_t output_lit = nodeToLiteral(bdds[output_var].getNode(), bdd_cache, aig_cache);
        aiger_add_output(aig, output_lit, m.outputs[output_var].c_str());
    }
    for (var_t latch_var = 0; latch_var < n_latches; latch_var++) {
        const literal_t latch_lit = nodeToLiteral(bdds[m.n_outputs + latch_var].getNode(), bdd_cache, aig_cache);
        std::stringstream latch_name_stream;
        latch_name_stream << "l" << latch_var;
        const std::string latch_name = latch_name_stream.str();
        aiger_add_latch(aig, latch2lit(latch_var), latch_lit, latch_name.c_str());
    }

    if (!keepBDDs) {
        bdds.clear();
        manager = Cudd();
    }
}

void AigerConstructor::executeAbcCommand(Abc_Frame_t* pAbc, const std::string command) const {
    if (Cmd_CommandExecute( pAbc, command.c_str())) {
        throw std::runtime_error("Cannot execute ABC command: " + command);
    }
}

// commands taken from 'alias compress2rs' from 'abc.rc' file
const std::vector<std::string> AigerConstructor::compressCommands ({
    "balance -l",
    "resub -K 6 -l",
    "rewrite -l",
    "resub -K 6 -N 2",
    "refactor -l",
    "resub -K 8 -l",
    "balance -l",
    "resub -K 8 -N 2 -l",
    "rewrite -l",
    "resub -K 10 -l",
    "rewrite -z -l",
    "resub -K 10 -N 2 -l",
    "balance -l",
    "resub -K 12 -l",
    "refactor -z -l",
    "resub -K 12 -N 2 -l",
    "balance -l",
    "rewrite -z -l",
    "balance -l"
});

void AigerConstructor::executeCompressCommands(Abc_Frame_t* pAbc) const {
    for (const auto& command : compressCommands) {
        executeAbcCommand(pAbc, command);
    }
}

int AigerConstructor::getAbcNetworkSize(Abc_Frame_t* pAbc) const {
    Abc_Ntk_t* pNtk = Abc_FrameReadNtk(pAbc);
    return Abc_NtkNodeNum(pNtk);
}

int AigerConstructor::getTmpFile(char* tmp_filename) const {
    boost::filesystem::path tmpfile_template_path = boost::filesystem::temp_directory_path() / "strix.XXXXXX";
    std::string tmpfile_template = tmpfile_template_path.string();
    std::strcpy(tmp_filename, tmpfile_template.c_str());
    int fd = mkstemp(tmp_filename);
    if (fd == -1) {
        throw std::runtime_error("Could not create temporary file: " + std::string(tmp_filename));
    }
    return fd;
}

void AigerConstructor::writeToAbc(Abc_Frame_t* pAbc) const {
    char tmp_filename[256];
    int fd = getTmpFile(tmp_filename);

    // write AIGER out to be read by ABC
    FILE* file = fdopen(fd, "w");
    if (file == nullptr) {
        throw std::runtime_error("Could not open temporary file: " + std::string(tmp_filename));
    }
    int write_result = write_aiger(file, true);
    fclose(file);
    if (write_result == 0) {
        throw std::runtime_error("Could not write AIGER circuit to file: " + std::string(tmp_filename));
    }

    std::stringstream read_command;
    read_command << "read_aiger " << tmp_filename;
    executeAbcCommand(pAbc, read_command.str());

    std::remove(tmp_filename);
}

void AigerConstructor::readFromAbc(Abc_Frame_t* pAbc) {
    char tmp_filename[256];
    int fd = getTmpFile(tmp_filename);

    std::stringstream write_command;
    write_command << "write_aiger -s " << tmp_filename;
    executeAbcCommand(pAbc, write_command.str());

    // read AIGER back, delete comments added by ABC
    FILE* file = fdopen(fd, "r");
    if (file == nullptr) {
        throw std::runtime_error("Could not open temporary file: " + std::string(tmp_filename));
    }
    const char* read_result = read_aiger(file);
    fclose(file);
    std::remove(tmp_filename);
    if (read_result != nullptr) {
        throw std::runtime_error("Could not read AIGER circuit from file: " + std::string(tmp_filename) + ": " + std::string(read_result));
    }
    aiger_delete_comments(aig);
}

void AigerConstructor::compressAiger(Abc_Frame_t* pAbc) {
    writeToAbc(pAbc);

    // compress until convergence
    int new_num_nodes = getAbcNetworkSize(pAbc);
    int old_num_nodes = new_num_nodes + 1;
    while (new_num_nodes > 0 && new_num_nodes < old_num_nodes) {
        executeCompressCommands(pAbc);
        old_num_nodes = new_num_nodes;
        new_num_nodes = getAbcNetworkSize(pAbc);
    }

    readFromAbc(pAbc);
}

void AigerConstructor::construct(const bool compress, const bool keepBDDs) {
    constructAiger(keepBDDs);

    if (compress) {
        // start the ABC framework
        Abc_Start();
        Abc_Frame_t* pAbc = Abc_FrameGetGlobalFrame();

        compressAiger(pAbc);

        // stop the ABC framework
        Abc_Stop();
    }
}

int aiger_put_wrapper (char ch, std::ostream* out) {
    out->put(ch);
    if (out->rdstate() == std::ios_base::goodbit) {
        return ch;
    }
    else {
        return EOF;
    }
}

void AigerConstructor::print_aiger(std::ostream& out, const bool binary) const {
    aiger_mode mode = binary ? aiger_binary_mode : aiger_ascii_mode;
    aiger_write_generic(aig, mode, &out, (aiger_put)aiger_put_wrapper);
}

int AigerConstructor::write_aiger(FILE* file, const bool binary) const {
    aiger_mode mode = binary ? aiger_binary_mode : aiger_ascii_mode;
    return aiger_write_to_file(aig, mode, file);
}

const char* AigerConstructor::read_aiger(FILE* file) {
    aiger_reset(aig);
    aig = aiger_init();

    return aiger_read_from_file(aig, file);
}

void AigerConstructor::print_dot(std::ostream& out) const {
    // use ABC for writing DOT file
    Abc_Start();
    Abc_Frame_t* pAbc = Abc_FrameGetGlobalFrame();

    writeToAbc(pAbc);

    char tmp_filename[256];
    int fd = getTmpFile(tmp_filename);
    std::stringstream dot_command;
    dot_command << "write_dot " << tmp_filename;
    executeAbcCommand(pAbc, dot_command.str());

    Abc_Stop();

    boost::iostreams::stream<boost::iostreams::file_descriptor_source> in(fd, boost::iostreams::close_handle);
    // filter title tags
    std::string line;
    int block_depth = 0;
    bool level_title_block = false;
    bool title_block = false;
    while(std::getline(in,line)) {
        std::string trimmed_line = boost::trim_copy(line);
        bool discard = false;

        if (boost::starts_with(trimmed_line, "{") || boost::ends_with(trimmed_line, "{")) {
            block_depth++;
            level_title_block = false;
            title_block = false;
        }
        if (boost::starts_with(trimmed_line, "}") || boost::ends_with(trimmed_line, "}")) {
            block_depth--;
            level_title_block = false;
            title_block = false;
        }
        if (block_depth >= 2 && level_title_block && title_block) {
            discard = true;
        }
        else {
            if (boost::starts_with(trimmed_line, "title1") || boost::starts_with(trimmed_line, "title2")) {
                title_block = true;
                discard = true;
            }
            else if (boost::starts_with(trimmed_line, "LevelTitle")) {
                level_title_block = true;
                if (boost::ends_with(trimmed_line, "Level0;")) {
                    std::stringstream line_in(line);
                    std::stringstream line_out;
                    std::string token;
                    while (line_in >> token) {
                        if (token == "LevelTitle1" || token == "LevelTitle2") {
                            line_in >> token;
                        }
                        else {
                            line_out << "  " << token;
                        }
                    }
                    line = line_out.str();
                }
                else {
                    discard = true;
                }
            }
        }
        if (!discard) {
            out << line << std::endl;
        }
    }
    in.close();

    std::remove(tmp_filename);
}

std::shared_ptr<AigerConstructor> AigerConstructor::findMinimalAiger(const mealy::MealyMachine& m, const bool use_labels, const bool use_minimal, const bool compress, const bool keepBDDs) {
    std::vector<std::shared_ptr<AigerConstructor>> configurations;
    configurations.reserve(3);

    if (use_labels) {
        configurations.push_back(std::shared_ptr<AigerConstructor>(new AigerConstructor(m, true, false)));
    }
    if (use_minimal) {
        configurations.push_back(std::shared_ptr<AigerConstructor>(new AigerConstructor(m, false, true)));
    }
    configurations.push_back(std::shared_ptr<AigerConstructor>(new AigerConstructor(m, false, false)));

    std::vector<std::thread> threads;
    for (const auto& conf : configurations) {
        std::thread construction_thread = std::thread(&AigerConstructor::construct, &*conf, false, keepBDDs);
        threads.push_back(std::move(construction_thread));
    }
    for (auto& thread : threads) {
        thread.join();
    }

    std::tuple<literal_t,literal_t> min =
        { std::numeric_limits<literal_t>::max(), std::numeric_limits<literal_t>::max() };
    for (const auto& conf : configurations) {
        min = std::min(min, conf->size());
    }

    if (compress) {
        // unfortunately the ABC library is not thread-safe, need to do compression sequentially
        Abc_Start();
        Abc_Frame_t* pAbc = Abc_FrameGetGlobalFrame();
        for (const auto& conf : configurations) {
            if (std::get<0>(min) >= std::get<0>(conf->size()) / 10) {
                // avoid time-consuming compression of huge circuits if there is already a much smaller solution
                conf->compressAiger(pAbc);
                min = std::min(min, conf->size());
            }
        }
        Abc_Stop();
    }

    std::shared_ptr<AigerConstructor> best;
    for (const auto& conf : configurations) {
        if (conf->size() == min) {
            best = conf;
            break;
        }
    }
    return best;
}

}
