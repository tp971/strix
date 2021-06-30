/*
 * DIMACSWriter.h
 *
 *  Created on: 02.03.2015
 *      Author: Andreas Abel
 *  Modified on: 17.05.2018
 *      Author: Philipp Meyer
 */
#pragma once

#include <vector>
#include <map>
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <sstream>
#include <sys/time.h>

#include "minisat/core/Solver.h"

#include "util/SpecSeq.h"
#include "Machine.h"

namespace mealy {

class DIMACSWriter {
  public:
    typedef int lit_t;

  private:
    int verbosity;

    Minisat::Solver solver;

    std::vector<lit_t> auxLiteralsMap;
    std::vector<lit_t> stateClassToLiteral;
    lit_t curLiteral;

    lit_t getStateLiteral(state_id_t state, state_id_t sClass, state_id_t numClasses);

    lit_t getAuxLiteral(state_id_t sClass);

    void addLitToClause(Minisat::vec<Minisat::Lit>& clause, lit_t lit);

    void computeReducedInputAlphabet(std::unordered_set<size_t>& reducedInputAlphabet, size_t numInputs, std::vector<std::vector<state_id_t> >& machineNextState, int verbosity);

  public:

    DIMACSWriter(int verbosity = 0);

    void buildCNF(std::vector<std::pair<state_id_t, state_id_t> >& literalToStateClass, state_id_t numClasses, std::vector<std::vector<state_id_t> >& machineNextState, std::vector<bool>& incompMatrix, std::vector<state_id_t>& pairwiseIncStates, size_t numInputs);

    /*
     * Check satisfiability of the constructed constraints.
     * Return 1 if the constraints are satisfiable,
     * 0 if the constraints and -1 if an error occurred.
     */
    int checkSatisfiability();
    std::vector<lit_t> getModel();

};

}
