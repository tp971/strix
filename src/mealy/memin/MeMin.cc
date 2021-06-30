/*
 * MemMin.cc
 *
 *  Created on: ??.??.2015
 *      Author: Andreas Abel
 *  Modified on: 17.05.2018
 *      Author: Philipp Meyer
 */
#include <vector>
#include <queue>
#include <algorithm>

#include "minisat/mtl/XAlloc.h"

#include "MeMin.h"
#include "util/SpecSeq.h"
#include "DIMACSWriter.h"
#include "MachineBuilder.h"

namespace mealy {

void computePredecessorMap(const machine_t& states, std::vector<std::unordered_map<SpecSeq<letter_t>,std::vector<state_id_t> > >& pred);
void computeIncompMatrix(const machine_t& states, std::vector<std::unordered_map<SpecSeq<letter_t>,std::vector<state_id_t> > >& pred, std::vector<bool>& incompMatrix);
std::vector<std::vector<bool> > getTransitivelyCompatibleStates(const machine_t& states, std::vector<bool>& incompMatrix);
void splitTransitions(const machine_t& states, int numInputBits, std::vector<bool>& incompMatrix, std::vector<std::vector<state_id_t> >& newNextStates, std::vector<std::vector<std::vector<SpecSeq<letter_t>>> >& newOutput, std::vector<SpecSeq<letter_t>>& inputIDToSpecSeq);
std::unordered_set<SpecSeq<letter_t>> getDisjointInputSet(const machine_t& states, int numInputBits, std::vector<bool>& eqClass);
void findPairwiseIncStates(std::vector<state_id_t>& pairwiseIncStates, std::vector<bool>& incompMatrix, state_id_t nStates);

int minimizeMachineMeMin(const machine_t& machine, state_id_t resetState, int numInputBits, int numOutputBits, machine_t& minimizedMachine, std::vector<std::vector<state_id_t> >& newStates, const int verbosity) {
    try {
        timeval start, end;
        gettimeofday(&start, 0);

        state_id_t nStates = machine.size();

        //predecessors for each state and input
        std::vector<std::unordered_map<SpecSeq<letter_t>,std::vector<state_id_t> >> pred(nStates);
        computePredecessorMap(machine, pred);

        gettimeofday(&end, 0);
        if (verbosity >= 2) std::cout << "Computing pred map: "<< (end.tv_sec*1e6 + end.tv_usec) - (start.tv_sec*1e6 + start.tv_usec) << " usec" << std::endl;
        gettimeofday(&start, 0);

        //0 if compatible, 1 if incompatible
        std::vector<bool> incompMatrix;
        incompMatrix.resize(nStates*nStates, false);
        computeIncompMatrix(machine, pred, incompMatrix);

        gettimeofday(&end, 0);
        if (verbosity >= 2) std::cout << "Computing IncompMatrix: "<< (end.tv_sec*1e6 + end.tv_usec) - (start.tv_sec*1e6 + start.tv_usec) << " usec" << std::endl;
        gettimeofday(&start, 0);

        std::vector<std::vector<state_id_t> > nextStatesMap(nStates);
        std::vector<std::vector<std::vector<SpecSeq<letter_t>>> > outputsMap(nStates);
        std::vector<SpecSeq<letter_t>> inputIDToSpecSeq;
        splitTransitions(machine, numInputBits, incompMatrix, nextStatesMap, outputsMap, inputIDToSpecSeq);

        gettimeofday(&end, 0);
        if (verbosity >= 2) std::cout << "Splitting transitions: "<< (end.tv_sec*1e6 + end.tv_usec) - (start.tv_sec*1e6 + start.tv_usec) << " usec" << std::endl;
        gettimeofday(&start, 0);

        gettimeofday(&start, 0);
        std::vector<state_id_t> pairwiseIncStates;
        findPairwiseIncStates(pairwiseIncStates, incompMatrix, nStates);
        gettimeofday(&end, 0);
        if (verbosity >= 2) std::cout << "Finding pairwise incomp states: "<< (end.tv_sec*1e6 + end.tv_usec) - (start.tv_sec*1e6 + start.tv_usec) << " usec" << std::endl;
        gettimeofday(&start, 0);

        for (state_id_t nClasses = pairwiseIncStates.size(); nClasses < nStates; nClasses++) {
            if (verbosity >= 2) std::cout << "Classes: " << nClasses << std::endl;

            //if literal i is true, and literalToStateClass[i]=(s,c) then state s is in class c
            std::vector<std::pair<state_id_t, state_id_t> > literalToStateClass;

            timeval start2, end2;
            gettimeofday(&start2, 0);

            DIMACSWriter writer;
            writer.buildCNF(literalToStateClass, nClasses, nextStatesMap, incompMatrix, pairwiseIncStates, inputIDToSpecSeq.size());

            gettimeofday(&end2, 0);
            if (verbosity >= 2) std::cout << "Building CNF: "<< (end2.tv_sec*1e6 + end2.tv_usec) - (start2.tv_sec*1e6 + start2.tv_usec) << " usec" << std::endl;
            gettimeofday(&start2, 0);

            int result = writer.checkSatisfiability();

            gettimeofday(&end2, 0);
            if (verbosity >= 2) std::cout << "Minisat: "<< (end2.tv_sec*1e6 + end2.tv_usec) - (start2.tv_sec*1e6 + start2.tv_usec) << " usec" << std::endl;

            if (result == -1) {
                return -1;
            }
            else if (result == 1) {
                gettimeofday(&end, 0);
                if (verbosity >= 2) std::cout << "Total time for SAT: "<< (end.tv_sec*1e6 + end.tv_usec) - (start.tv_sec*1e6 + start.tv_usec) << " usec" << std::endl;
                gettimeofday(&start, 0);

                std::vector<DIMACSWriter::lit_t> dimacsOutput = writer.getModel();

                state_id_t newResetState = NONE_STATE;

                std::vector<std::vector<state_id_t> > newMachineNextStatesMap(nClasses);
                std::vector<std::vector<std::vector<SpecSeq<letter_t>>> > newMachineOutputsMap(nClasses);
                if (!buildMachine(newMachineNextStatesMap, newMachineOutputsMap, newResetState, newStates, nClasses, dimacsOutput, literalToStateClass, nextStatesMap, outputsMap, resetState, inputIDToSpecSeq.size(), verbosity)) {
                    return -1;
                }

                gettimeofday(&end, 0);
                if (verbosity >= 2) std::cout << "Building machine: "<< (end.tv_sec*1e6 + end.tv_usec) - (start.tv_sec*1e6 + start.tv_usec) << " usec" << std::endl;
                gettimeofday(&start, 0);

                constructMachine(newMachineNextStatesMap, newMachineOutputsMap, newResetState, newStates, inputIDToSpecSeq, minimizedMachine);

                gettimeofday(&end, 0);
                if (verbosity >= 2) std::cout << "Constructing minimized machine: "<< (end.tv_sec*1e6 + end.tv_usec) - (start.tv_sec*1e6 + start.tv_usec) << " usec" << std::endl;

                return 1;
            }
        }
    }
    catch (const Minisat::OutOfMemoryException&) {
        std::cerr << "Error: MiniSat out of memory." << std::endl;
        return -1;
    }
    return 0;
}

void computePredecessorMap(const machine_t& states, std::vector<std::unordered_map<SpecSeq<letter_t>,std::vector<state_id_t> > >& pred) {
    for (state_id_t s = 0; s < states.size(); s++) {
        const std::vector<Transition>& tMap = states[s];
        for (auto it=tMap.cbegin(); it!=tMap.cend(); it++) {
            pred[it->nextState][it->input].push_back(s);
        }
    }
}

void propagateIncompStates(state_id_t s1_initial, state_id_t s2_initial, state_id_t nStates, std::vector<std::unordered_map<SpecSeq<letter_t>, std::vector<state_id_t> > >& pred, std::vector<bool>& incompMatrix) {
    std::queue<std::pair<state_id_t,state_id_t>> worklist;
    worklist.push({s1_initial, s2_initial});

    while(!worklist.empty()) {
        const std::pair<state_id_t,state_id_t> pair = std::move(worklist.front());
        worklist.pop();
        const state_id_t s1 = pair.first;
        const state_id_t s2 = pair.second;

        std::unordered_map<SpecSeq<letter_t>, std::vector<state_id_t> >& pred1 = pred[s1];
        std::unordered_map<SpecSeq<letter_t>, std::vector<state_id_t> >& pred2 = pred[s2];

        for (auto it1=pred1.cbegin(); it1!=pred1.cend(); it1++) {
            const SpecSeq<letter_t>& input1 = it1->first;
            const std::vector<state_id_t>& predStates1 = it1->second;

            for (auto it2=pred2.cbegin(); it2!=pred2.cend(); it2++) {
                const SpecSeq<letter_t>& input2 = it2->first;

                if (input1.isDisjoint(input2)) continue;

                const std::vector<state_id_t>& predStates2 = it2->second;

                for (state_id_t i1 = 0; i1<predStates1.size(); i1++) {
                    state_id_t predS1 = predStates1[i1];
                    for (state_id_t i2 = 0; i2<predStates2.size(); i2++) {
                        state_id_t predS2 = predStates2[i2];
                        if (incompMatrix[ai(predS1,predS2,nStates)]) continue;

                        incompMatrix[ai(predS1,predS2,nStates)]=true;
                        incompMatrix[ai(predS2,predS1,nStates)]=true;

                        worklist.push({predS1, predS2});
                    }
                }
            }
        }
    }
}

void computeIncompMatrix(const machine_t& states, std::vector<std::unordered_map<SpecSeq<letter_t>, std::vector<state_id_t> > >& pred, std::vector<bool>& incompMatrix) {
    state_id_t nStates = states.size();

    for (state_id_t s1 = 0; s1 < nStates; s1++) {
        const std::vector<Transition>& succMap1 = states[s1];
        for (state_id_t s2 = s1; s2 < nStates; s2++) {
            if (incompMatrix[ai(s1,s2,nStates)]) continue;

            const std::vector<Transition>& succMap2 = states[s2];

            for (auto it1=succMap1.cbegin(); it1!=succMap1.cend(); it1++) {
                const SpecSeq<letter_t>& input1 = it1->input;

                bool incompOutputFound = false;
                for (auto it2=succMap2.cbegin(); it2!=succMap2.cend(); it2++) {
                    const SpecSeq<letter_t>& input2 = it2->input;

                    if (input1.isDisjoint(input2)) continue;

                    bool compatible = false;
                    for (const SpecSeq<letter_t>& o1 : it1->output) {
                        for (const SpecSeq<letter_t>& o2 : it2->output) {
                            if (o1.isCompatible(o2)) {
                                compatible = true;
                                break;
                            }
                        }
                        if (compatible) {
                            break;
                        }
                    }
                    if (compatible) {
                        continue;
                    }

                    incompMatrix[ai(s1,s2,nStates)] = true;
                    incompMatrix[ai(s2,s1,nStates)] = true;

                    propagateIncompStates(s1, s2, nStates, pred, incompMatrix);

                    incompOutputFound = true;
                    break;
                }
                if (incompOutputFound) break;
            }
        }
    }
}

//partitions the set of states into equivalence classes, s.t. two states are in the same class if they are transitively compatible
//ret[i][s]==true iff state s is in class i
std::vector<std::vector<bool> > getTransitivelyCompatibleStates(const machine_t& states, std::vector<bool>& incompMatrix) {
    std::vector<std::vector<bool> > ret;

    state_id_t nStates = states.size();
    std::vector<bool> processedStates;
    processedStates.resize(nStates, false);

    for (state_id_t s = 0; s < nStates; s++) {
        if (processedStates[s]) continue;
        processedStates[s] = true;

        std::vector<bool> curSet;
        curSet.resize(nStates, 0);
        curSet[s]=true;

        std::queue<state_id_t> worklist;
        worklist.push(s);

        while (!worklist.empty()) {
            state_id_t curS = worklist.front();
            worklist.pop();

            for (state_id_t i = s; i < nStates; i++) {
                if (processedStates[i]) continue;
                if (incompMatrix[ai(curS,i,nStates)]) continue;

                worklist.push(i);
                processedStates[i] = true;
                curSet[i]=true;
            }
        }
        ret.push_back(curSet);
    }

    return ret;
}

//compatible states must not have transitions with overlapping inputs
void splitTransitions(const machine_t& states, int numInputBits, std::vector<bool>& incompMatrix, std::vector<std::vector<state_id_t> >& newNextStates, std::vector<std::vector<std::vector<SpecSeq<letter_t>>> >& newOutput, std::vector<SpecSeq<letter_t>>& inputIDToSpecSeq) {
    std::vector<std::vector<bool> > tcs = getTransitivelyCompatibleStates(states, incompMatrix);

    std::vector<std::unordered_set<SpecSeq<letter_t>> > disjInputsForTCS(tcs.size());

    std::unordered_map<SpecSeq<letter_t>, size_t> specSeqToInputID;

    for (state_id_t i = 0; i < tcs.size(); i++) {
        std::unordered_set<SpecSeq<letter_t>> disjInputs = getDisjointInputSet(states, numInputBits, tcs[i]);
        disjInputsForTCS[i] = disjInputs;

        for (auto it=disjInputs.cbegin(); it!=disjInputs.cend(); it++) {
            const SpecSeq<letter_t> disjInput = *it;

            if (specSeqToInputID.count(disjInput)>0) continue;

            specSeqToInputID[disjInput] = inputIDToSpecSeq.size();
            inputIDToSpecSeq.push_back(disjInput);
        }
    }

    for (size_t i = 0; i < tcs.size(); i++) {
        std::unordered_set<SpecSeq<letter_t>>& disjInputs = disjInputsForTCS[i];

        std::vector<bool>& tcsi = tcs[i];
        for (state_id_t curTcs = 0; curTcs < states.size(); curTcs++) {
            if (!tcsi[curTcs]) continue;
            const std::vector<Transition>& curMap = states[curTcs];

            std::vector<state_id_t>& curNextState = newNextStates[curTcs];
            std::vector<std::vector<SpecSeq<letter_t>>>& curOutput = newOutput[curTcs];
            curNextState.resize(inputIDToSpecSeq.size());
            curOutput.resize(inputIDToSpecSeq.size());
            for (state_id_t j = 0; j < inputIDToSpecSeq.size(); j++) {
                curNextState[j] = NONE_STATE;
            }

            for (auto mIt=curMap.cbegin(); mIt!=curMap.cend(); mIt++) {
                const SpecSeq<letter_t>& input = mIt->input;

                if (disjInputs.count(input)>0) {
                    size_t inputID = specSeqToInputID[input];
                    curNextState[inputID] = mIt->nextState;
                    curOutput[inputID] = mIt->output;
                } else {
                    for (auto sIt=disjInputs.cbegin(); sIt!=disjInputs.cend(); sIt++) {
                        const SpecSeq<letter_t>& disjInput = *sIt;
                        if (disjInput.isSubset(input)) {
                            size_t inputID = specSeqToInputID[disjInput];
                            curNextState[inputID] = mIt->nextState;
                            curOutput[inputID] = mIt->output;
                        }
                    }
                }
            }
        }
    }
}

//computes a set of non-overlapping input sequences s.t. all transitions for states in eqClass are covered
std::unordered_set<SpecSeq<letter_t>> getDisjointInputSet(const machine_t& states, int numInputBits, std::vector<bool>& eqClass) {
    state_id_t nStates = states.size();
    std::unordered_set<SpecSeq<letter_t>, std::hash<SpecSeq<letter_t>>> disjointInputs;

    state_id_t curS;
    for (curS = 0; curS < nStates; curS++) {
        if (eqClass[curS]) break;
    }
    if (curS >= nStates) return std::unordered_set<SpecSeq<letter_t>>();

    bool nonFullySpecInputFound = false;

    const std::vector<Transition>& firstMap = states[curS];
    for (auto mIp=firstMap.cbegin(); mIp!=firstMap.cend(); mIp++) {
        SpecSeq<letter_t> input = mIp->input;
        if (!input.isFullySpecified()) nonFullySpecInputFound=true;
        disjointInputs.insert(input);
    }


    std::unordered_set<SpecSeq<letter_t>, std::hash<SpecSeq<letter_t>>> alreadyInQueue;
    std::queue<SpecSeq<letter_t>> remainingInputs;
    for (; curS<nStates; curS++) {
        if (!eqClass[curS]) continue;
        const std::vector<Transition>& curMap = states[curS];
        for (auto mIp=curMap.cbegin(); mIp!=curMap.cend(); mIp++) {
            SpecSeq<letter_t> input = mIp->input;
            if (!input.isFullySpecified()) nonFullySpecInputFound=true;
            if (alreadyInQueue.count(input)==0) {
                remainingInputs.push(input);
                alreadyInQueue.insert(input);
            }
        }
    }

    if (!nonFullySpecInputFound) {
        while (!remainingInputs.empty()) {
            SpecSeq<letter_t> curInput = remainingInputs.front();
            remainingInputs.pop();
            disjointInputs.insert(curInput);
        }
        std::unordered_set<SpecSeq<letter_t>> ret;
        for (auto it=disjointInputs.cbegin(); it!=disjointInputs.cend(); it++) {
            ret.insert(*it);
        }
        return ret;
    }

    while (!remainingInputs.empty()) {
        SpecSeq<letter_t> curInput = remainingInputs.front();
        remainingInputs.pop();

        if (disjointInputs.count(curInput)>0) continue;

        bool intersectingInputFound=false;
        for (auto dIt=disjointInputs.cbegin(); dIt != disjointInputs.cend(); dIt++) {
            SpecSeq<letter_t> disjInput = *dIt;

            if (!disjInput.isDisjoint(curInput)) {
                intersectingInputFound = true;

                if (curInput.isSubset(disjInput)) {
                    SpecSeq<letter_t> inters = disjInput.intersect(curInput);
                    std::vector<SpecSeq<letter_t>> diff = disjInput.diff(curInput, numInputBits);
                    disjointInputs.erase(disjInput);
                    disjointInputs.insert(SpecSeq<letter_t>(inters));
                    for (size_t i=0; i < diff.size(); i++) {
                        disjointInputs.insert(SpecSeq<letter_t>(diff[i]));
                    }
                } else if (disjInput.isSubset(curInput)) {
                    std::vector<SpecSeq<letter_t>> diff = curInput.diff(disjInput, numInputBits);

                    for (size_t i=0; i < diff.size(); i++) {
                        remainingInputs.push(SpecSeq<letter_t>(diff[i]));
                    }
                } else {
                    SpecSeq<letter_t> inters = disjInput.intersect(curInput);
                    std::vector<SpecSeq<letter_t>> diff = disjInput.diff(curInput, numInputBits);

                    disjointInputs.insert(SpecSeq<letter_t>(inters));

                    for (size_t i=0; i < diff.size(); i++) {
                        disjointInputs.insert(SpecSeq<letter_t>(diff[i]));
                    }

                    std::vector<SpecSeq<letter_t>> diff2 = curInput.diff(disjInput, numInputBits);
                    for (size_t i=0; i < diff2.size(); i++) {
                        remainingInputs.push(SpecSeq<letter_t>(diff2[i]));
                    }

                    disjointInputs.erase(disjInput);
                }
                break;
            }
        }
        if (!intersectingInputFound) {
            disjointInputs.insert(curInput);
        }
    }

    std::unordered_set<SpecSeq<letter_t>> ret;
    for (auto it=disjointInputs.cbegin(); it!=disjointInputs.cend(); it++) {
        ret.insert(*it);
    }
    return ret;
}

class incStateComp {
    std::vector<state_id_t> nIncomp;
public:
    incStateComp(std::vector<bool>& incompMatrix, state_id_t nStates) :
        nIncomp(nStates)
    {
        for (state_id_t i=0; i<nStates; i++) {
            state_id_t c=0;
            for (state_id_t j=0; j<nStates; j++) {
                if (incompMatrix[ai(i,j,nStates)]) c++;
            }
            nIncomp[i] = c;
        }
    }

    bool operator() (state_id_t i, state_id_t j) {
        return (nIncomp[i]>nIncomp[j]);
    }
};

void findPairwiseIncStates(std::vector<state_id_t>& pairwiseIncStates, std::vector<bool>& incompMatrix, state_id_t nStates) {
    incStateComp comp(incompMatrix, nStates);

    std::vector<state_id_t> states;
    for (state_id_t i=0; i<nStates; i++) {
        states.push_back(i);
    }

    std::sort(states.begin(), states.end(), comp);

    for (state_id_t i=0; i < nStates; i++) {
        state_id_t s1 = states[i];
        bool compStateFound = false;
        for (state_id_t j=0; j < pairwiseIncStates.size(); j++) {
            state_id_t s2 = pairwiseIncStates[j];
            if (!incompMatrix[ai(s1,s2,nStates)]) {
                compStateFound = true;
                break;
            }
        }
        if (compStateFound) continue;
        pairwiseIncStates.push_back(s1);
    }
}

}
