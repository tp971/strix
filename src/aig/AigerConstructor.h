#pragma once

#include <iostream>
#include <fstream>
#include <unordered_map>

#include "cuddObj.hh"
#include "cuddInt.h"
extern "C" {
#include "aiger/aiger.h"
#include "abc/src/misc/util/abc_namespaces.h"
#include "abc/src/base/main/abcapis.h"
#include "abc/src/base/abc/abc.h"
#include "abc/src/base/main/main.h"
}

#include "mealy/MealyMachine.h"
#include "util/SpecSeq.h"

namespace aig {
    typedef std::vector<std::vector<mealy::Transition> > transition_vec;

    typedef uint64_t literal_t;
    typedef uint32_t var_t;

    struct literal_pair_t {
        literal_t lhs;
        literal_t rhs;
        literal_pair_t(const literal_t lhs, const literal_t rhs) :
            lhs(lhs), rhs(rhs)
        {}

        bool operator==(const literal_pair_t& other) const {
            return lhs == other.lhs && rhs == other.rhs;
        }
    };
}

template <>
struct std::hash<aig::literal_pair_t> {
    std::size_t operator()(const aig::literal_pair_t& pair) const {
        std::size_t seed = 0;
        boost::hash_combine(seed, pair.lhs);
        boost::hash_combine(seed, pair.rhs);
        return seed;
    }
};

namespace aig {
    class AigerConstructor {
        private:
            static const std::vector<std::string> compressCommands;

            const mealy::MealyMachine& m;
            const bool use_labels;
            const bool minimize;

            aiger* aig;
            size_t n_states;
            size_t n_latches;
            size_t and_gate_index;

            Cudd manager;
            std::vector<BDD> bdds;

            void constructBDDs();
            BDD constructBDDForVar(const var_t var,
                    const transition_vec& transitions);
            literal_t nodeToLiteral(DdNode* node,
                    std::unordered_map<DdNode*, literal_t>& bdd_cache,
                    std::unordered_map<literal_pair_t, literal_t>& aig_cache);

            inline literal_t input2lit(const var_t input_var) const {
                return aiger_var2lit(1 + input_var);
            }
            inline literal_t latch2lit(const var_t latch_var) const {
                return aiger_var2lit(1 + m.n_inputs + latch_var);
            }
            inline literal_t input2bddvar(const var_t input_var) const {
                return input_var;
            }
            inline literal_t latch2bddvar(const var_t latch_var) const {
                return m.n_inputs + latch_var;
            }
            inline literal_t bddvar2lit(const var_t var) const {
                return aiger_var2lit(1 + var);
            }

            literal_t add_and(literal_t lhs, literal_t rhs, std::unordered_map<literal_pair_t, literal_t>& aig_cache);

            void executeAbcCommand(Abc_Frame_t* pAbc, const std::string command) const;
            void executeCompressCommands(Abc_Frame_t* pAbc) const;
            int getAbcNetworkSize(Abc_Frame_t* pAbc) const;

            void constructAiger(const bool keepBDDs);
            void compressAiger(Abc_Frame_t* pAbc);

            int getTmpFile(char* tmp_filename) const;
            void writeToAbc(Abc_Frame_t* pAbc) const;
            void readFromAbc(Abc_Frame_t* pAbc);

        public:
            AigerConstructor(const mealy::MealyMachine& m, const bool use_labels, const bool minimize);
            ~AigerConstructor();

            void construct(const bool compress, const bool keepBDDs);
            void print_aiger(std::ostream& out, const bool binary = false) const;
            void print_dot(std::ostream& out) const;
            int write_aiger(FILE* file, const bool binary = false) const;
            const char* read_aiger(FILE* file);

            void printBDDs(std::ostream& out) const;

            inline literal_t num_ands() { return aig->num_ands; }
            inline literal_t num_latches() { return aig->num_latches; }
            inline std::tuple<literal_t,literal_t> size() {
                return std::tuple<literal_t,literal_t>(num_ands(), num_latches());
            }

            static std::shared_ptr<AigerConstructor> findMinimalAiger(const mealy::MealyMachine& m, const bool use_labels, const bool use_minimal, const bool compress, const bool keepBDDs);
    };
}
