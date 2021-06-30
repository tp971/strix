/*
 * DIMACSWriter.cc
 *
 *  Created on: 02.03.2015
 *      Author: Andreas Abel
 *  Modified on: 17.05.2018
 *      Author: Philipp Meyer
 */

#include "DIMACSWriter.h"

namespace mealy {

DIMACSWriter::DIMACSWriter(int verbosity) : verbosity(verbosity) { }

void DIMACSWriter::computeReducedInputAlphabet(std::unordered_set<size_t>& reducedInputAlphabet, size_t numInputs, std::vector<std::vector<state_id_t> >& machineNextState, int verbosity) {
    std::unordered_map<size_t, std::vector<std::vector<state_id_t> > > hashmap;

    for (size_t input = 0; input < numInputs; input++) {

        size_t hash = 0;
        std::vector<state_id_t> nextStatesList;

        for (state_id_t i=0; i < machineNextState.size(); i++) {
            std::vector<state_id_t>& curNextStates = machineNextState[i];
            state_id_t succ = curNextStates[input];

            if (succ == NONE_STATE) {
                nextStatesList.push_back(NONE_STATE);
                hash = 31*hash;
            } else {
                nextStatesList.push_back(succ);
                hash = 31*hash + succ;
            }
        }

        const auto hashmapEntry = hashmap.find(hash);

        if (hashmapEntry == hashmap.end()) {
            hashmap[hash].push_back(nextStatesList);
            reducedInputAlphabet.insert(input);
        } else {
            std::vector<std::vector<state_id_t> >& statesListsWithSameHash = hashmapEntry->second;

            bool contained = false;

            for (auto it = statesListsWithSameHash.cbegin(); it != statesListsWithSameHash.cend(); it++) {
                const std::vector<state_id_t>& curStateList = *it;

                bool equal = true;
                for (state_id_t i = 0; i < machineNextState.size(); i++) {
                    if (nextStatesList[i] != curStateList[i]) {
                        equal = false;
                        break;
                    }
                }

                if (equal) {
                    contained = true;
                    break;
                }
            }

            if (!contained) {
                hashmap[hash].push_back(nextStatesList);
                reducedInputAlphabet.insert(input);
            }
        }
    }
}

DIMACSWriter::lit_t DIMACSWriter::getStateLiteral(state_id_t state, state_id_t sClass, state_id_t numClasses) {
    state_id_t key = ai(state, sClass, numClasses);

    lit_t retLiteral = stateClassToLiteral[key];
    if (retLiteral == -1) {
        retLiteral = curLiteral;
        stateClassToLiteral[key] = curLiteral++;
    }

    return retLiteral;
}

DIMACSWriter::lit_t DIMACSWriter::getAuxLiteral(state_id_t sClass) {
    lit_t retLiteral = auxLiteralsMap[sClass];
    if (retLiteral == -1) {
        retLiteral = curLiteral;
        auxLiteralsMap[sClass] = curLiteral++;
    }

    return retLiteral;
}

void DIMACSWriter::addLitToClause(Minisat::vec<Minisat::Lit>& clause, lit_t lit) {
    lit_t var = abs(lit)-1;
    while (var >= (solver.nVars())) {
        solver.newVar();
    }
    clause.push((lit>0) ? Minisat::mkLit(var) : ~Minisat::mkLit(var));
}

void DIMACSWriter::buildCNF(std::vector<std::pair<state_id_t, state_id_t> >& literalToStateClass, state_id_t numClasses, std::vector<std::vector<state_id_t> >& machineNextState, std::vector<bool>& incompMatrix, std::vector<state_id_t>& pairwiseIncStates, size_t numInputs) {
    state_id_t nStates = machineNextState.size();
    curLiteral = 1;

    stateClassToLiteral.resize(nStates*numClasses);
    for (state_id_t i = 0; i < nStates*numClasses; i++) stateClassToLiteral[i] = -1;

    auxLiteralsMap.resize(numClasses);

    //add pairwise incompatible states to different classes
    for (state_id_t i=0; i < pairwiseIncStates.size(); i++) {
        Minisat::vec<Minisat::Lit> clause;
        state_id_t s = pairwiseIncStates[i];
        addLitToClause(clause, getStateLiteral(s,i,numClasses));
        solver.addClause(clause);
    }

    std::vector<std::vector<state_id_t> > statesThatCanBeInClass;
    statesThatCanBeInClass.resize(numClasses);
    for (state_id_t i = 0; i < numClasses; i++) {
        std::vector<state_id_t>& curVector = statesThatCanBeInClass[i];
        for (state_id_t s = 0; s < nStates; s++) {
            if (i < pairwiseIncStates.size() && incompMatrix[ai(s, pairwiseIncStates[i], nStates)]) continue;
            curVector.push_back(s);
        }
    }

    //each state must be in at least one class
    for (state_id_t s = 0; s < nStates; s++) {
        Minisat::vec<Minisat::Lit> clause;
        for (state_id_t i = 0; i < numClasses; i++) {
            if (i < pairwiseIncStates.size() && incompMatrix[ai(s, pairwiseIncStates[i], nStates)]) continue;
            addLitToClause(clause, getStateLiteral(s,i,numClasses));
        }
        solver.addClause(clause);
    }

    //incompatible states must not be in the same class
    for (state_id_t i =0; i < pairwiseIncStates.size(); i++) {
        state_id_t s = pairwiseIncStates[i];

        for (state_id_t incompS = 0; incompS < nStates; incompS++) {
            if (!incompMatrix[ai(s, incompS, nStates)]) continue;
            Minisat::vec<Minisat::Lit> clause;
            addLitToClause(clause, -getStateLiteral(incompS,i,numClasses));
            solver.addClause(clause);
        }
    }

    for (state_id_t s = 0; s < nStates; s++) {
        for (state_id_t i = 0; i < numClasses; i++) {
            if (i < pairwiseIncStates.size() && incompMatrix[ai(s, pairwiseIncStates[i], nStates)]) continue;
            for (state_id_t incompS = s+1; incompS<nStates; incompS++) {
                if (!incompMatrix[ai(s, incompS, nStates)]) continue;
                Minisat::vec<Minisat::Lit> clause;
                addLitToClause(clause, -getStateLiteral(s,i,numClasses));
                addLitToClause(clause, -getStateLiteral(incompS,i,numClasses));
                solver.addClause(clause);
            }
        }
    }

    std::unordered_set<size_t> reducedInputAlphabet;
    computeReducedInputAlphabet(reducedInputAlphabet, numInputs, machineNextState, verbosity);

    timeval start, end;
    gettimeofday(&start, 0);

    std::vector<bool> possibleSuccClasses;
    possibleSuccClasses.resize(numClasses,false);

    //closure constraints
    for (size_t a = 0; a < numInputs; a++) {
        if (reducedInputAlphabet.count(a)==0) continue;

        for (state_id_t i = 0; i < numClasses; i++) {
            //clear auxLiteralsMap and possibleSuccClasses
            for (state_id_t j = 0; j < numClasses; j++) {
                auxLiteralsMap[j] = -1;
            }
            possibleSuccClasses.assign(numClasses,false);

            state_id_t smallestSuccClass = numClasses+1;
            state_id_t largestSuccClass = 0;

            std::vector<state_id_t> statesThatCanBeInClassI = statesThatCanBeInClass[i];
            for (auto sIt = statesThatCanBeInClassI.cbegin(); sIt!=statesThatCanBeInClassI.cend(); sIt++) {
                state_id_t s = *sIt;
                state_id_t succS = machineNextState[s][a];
                if (succS == NONE_STATE) continue;

                for (state_id_t j = 0; j < numClasses; j++) {
                    if (j < pairwiseIncStates.size() && incompMatrix[ai(succS, pairwiseIncStates[j], nStates)]) continue;
                    possibleSuccClasses[j]=true;
                    if (j < smallestSuccClass) smallestSuccClass=j;
                    if (j > largestSuccClass) largestSuccClass=j;
                }
            }
            Minisat::vec<Minisat::Lit> clause;

            //auxOr
            for (state_id_t j = smallestSuccClass; j <= largestSuccClass; j++) {
                if (possibleSuccClasses[j]) {
                    addLitToClause(clause, getAuxLiteral(j));
                }
            }

            if (clause.size() == 0) continue;
            solver.addClause(clause);

            for (auto sIt = statesThatCanBeInClassI.cbegin(); sIt != statesThatCanBeInClassI.cend(); sIt++) {
                state_id_t s = *sIt;
                state_id_t succS = machineNextState[s][a];
                if (succS == NONE_STATE) continue;

                for (state_id_t j = smallestSuccClass; j <= largestSuccClass; j++) {
                    if (!possibleSuccClasses[j]) continue;

                    Minisat::vec<Minisat::Lit> clause2;
                    addLitToClause(clause2, -getAuxLiteral(j));
                    addLitToClause(clause2, -getStateLiteral(s,i,numClasses));
                    addLitToClause(clause2, getStateLiteral(succS,j,numClasses));
                    solver.addClause(clause2);
                }
            }
        }
    }

    gettimeofday(&end, 0);
    if (verbosity>=1) std::cout << "Closure Constraints: "<< (end.tv_usec-start.tv_usec) << " usec" << std::endl;

    std::pair<state_id_t, state_id_t> defaultPair(NONE_STATE, NONE_STATE);

    literalToStateClass.resize(curLiteral);
    for (lit_t i = 0; i < curLiteral; i++) literalToStateClass[i]=defaultPair;

    for (state_id_t i = 0; i < nStates; i++) {
        for (state_id_t j = 0; j < numClasses; j++) {
            if (stateClassToLiteral[ai(i,j,numClasses)] != -1) {
                literalToStateClass[stateClassToLiteral[ai(i,j,numClasses)]] = {i,j};
            }
        }
    }
}

int DIMACSWriter::checkSatisfiability() {
    Minisat::vec<Minisat::Lit> dummy;
    Minisat::lbool ret = solver.solveLimited(dummy);
    if (verbosity>0) std::cout << (ret == Minisat::l_True ? "SATISFIABLE\n" : ret == Minisat::l_False ? "UNSATISFIABLE\n" : "INDETERMINATE\n");

    if (ret == Minisat::l_True) {
        return 1;
    }
    else if (ret == Minisat::l_False) {
        return 0;
    }
    else {
        std::cerr << "Error: MiniSat returned indeterminate result" << std::endl;
        return -1;
    }
}

std::vector<DIMACSWriter::lit_t> DIMACSWriter::getModel() {
    std::vector<lit_t> dimacsOutput;
    for (lit_t i = 0; i < solver.nVars(); i++) {
        if (solver.model[i] != Minisat::l_Undef) {
            lit_t lit = i+1;
            if (solver.model[i]==Minisat::l_False) lit = -lit;
            dimacsOutput.push_back(lit);
        }
    }
    return dimacsOutput;
}

}
