/*
 * MachineBuilder.cc
 *
 *  Created on: 06.03.2015
 *      Author: Andreas Abel
 *  Modified on: 17.05.2018
 *      Author: Philipp Meyer
 */
#include "MachineBuilder.h"

namespace mealy {

bool buildMachine(std::vector<std::vector<state_id_t> >& newMachineNextState, std::vector<std::vector<std::vector<SpecSeq<letter_t>>> >& newMachineOutput, state_id_t& newResetState, std::vector<std::vector<state_id_t> >& newStates, state_id_t nClasses, std::vector<int>& dimacsOutput, std::vector<std::pair<state_id_t, state_id_t> >& literalToStateClass, std::vector<std::vector<state_id_t> >& origMachineNextState, std::vector<std::vector<std::vector<SpecSeq<letter_t>>> >& origMachineOutput, state_id_t origResetState, size_t numInputs, const int verbosity) {
    newStates.resize(nClasses);
    std::vector<std::vector<bool> > classesForOrigState(origMachineNextState.size());
    for (state_id_t i=0; i<origMachineNextState.size(); i++) classesForOrigState[i].resize(nClasses);

    newMachineNextState.resize(nClasses);
    newMachineOutput.resize(nClasses);

    for (size_t litI = 0; litI < dimacsOutput.size(); litI++) {
        int lit = dimacsOutput[litI];
        if (lit <= 0) continue;

        std::pair<state_id_t,state_id_t>& stateClass = literalToStateClass[lit];
        if (stateClass.first == NONE_STATE) continue;

        newStates[stateClass.second].push_back(stateClass.first);

        classesForOrigState[stateClass.first][stateClass.second]=true;

        if (stateClass.first == origResetState) {
            newResetState = stateClass.second;
        }
    }

    for (state_id_t stateI = 0; stateI < nClasses; stateI++) {
        std::vector<state_id_t>& state = newStates[stateI];

        newMachineNextState[stateI].resize(numInputs);
        newMachineOutput[stateI].resize(numInputs);

        for (size_t a = 0; a < numInputs; a++) {
            newMachineNextState[stateI][a] = NONE_STATE;
            std::vector<state_id_t> succ;
            std::vector<SpecSeq<letter_t>> output;

            for (auto stateIt = state.cbegin(); stateIt != state.cend(); stateIt++) {
                state_id_t oldState = *stateIt;

                state_id_t nextState = origMachineNextState[oldState][a];
                if (nextState == NONE_STATE) continue;


                if (succ.empty()) {
                    output = origMachineOutput[oldState][a];
                } else {
                    std::vector<SpecSeq<letter_t>> origOutput = origMachineOutput[oldState][a];
                    std::vector<SpecSeq<letter_t>> newOutputs;
                    for (const SpecSeq<letter_t>& o1 : output) {
                        for (const SpecSeq<letter_t>& o2 : origOutput) {
                            // TODO: use BDDs for Mealy machine outputs
                            // and conjunction for compatibility and intersection
                            if (o1 == o2) {
                                newOutputs.push_back(o1);
                            }
                            else if (o1.isCompatible(o2)) {
                                newOutputs.push_back(o1.intersect(o2));
                            }
                        }
                    }
                    output = newOutputs;
                    if (output.empty()) {
                        // TODO: this could happen when combining (-0, 0- and 10+01)
                        if (verbosity >= 1) {
                            std::cerr << "Could not combine outputs" << std::endl;
                        }
                        return false;
                    }
                }

                succ.push_back(nextState);
            }

            if (succ.empty()) continue;

            //find class that contains all successors
            state_id_t firstSucc = succ[0];
            std::vector<bool>& classesForFirstSucc = classesForOrigState[firstSucc];

            for (state_id_t succClass = 0; succClass < nClasses; succClass++) {
                if (!classesForFirstSucc[succClass]) continue;

                bool classForAllStates = true;
                for (state_id_t succI = 1; succI < succ.size(); succI++) {
                    state_id_t otherSucc = succ[succI];

                    if (!classesForOrigState[otherSucc][succClass]) {
                        classForAllStates=false;
                        break;
                    }
                }

                if (classForAllStates) {
                    newMachineNextState[stateI][a] = succClass;
                    newMachineOutput[stateI][a] = output;
                    break;
                }

                if (succClass + 1 == nClasses) {
                    if (verbosity >= 1) {
                        std::cerr << "No successor class found" << std::endl;
                    }
                    return false;
                }
            }
        }
    }
    return true;
}

void constructMachine(std::vector<std::vector<state_id_t> >& machineNextState, std::vector<std::vector<std::vector<SpecSeq<letter_t>>> >& machineOutput, state_id_t resetState, std::vector<std::vector<state_id_t> >& stateMapping, std::vector<SpecSeq<letter_t>>& inputIDToSpecSeq, machine_t& newMachine) {
    const state_id_t numStates = machineNextState.size();

    std::vector<std::vector<state_id_t> > newStateMapping(numStates);

    // reorder states so that reset state is again state 0
    std::vector<state_id_t> stateMap(numStates);
    state_id_t counter = 0;
    for (state_id_t state = 0; state < numStates; state++) {
        state_id_t s;
        if (state == resetState) {
            s = 0;
        }
        else {
            counter++;
            s = counter;
        }
        stateMap[state] = s;
        newStateMapping[s] = std::move(stateMapping[state]);
    }
    newStateMapping.swap(stateMapping);

    newMachine.resize(numStates);

    for (state_id_t state = 0; state < numStates; state++) {
        state_id_t curState = stateMap[state];

        std::vector<state_id_t>& curNextState = machineNextState[state];
        std::vector<std::vector<SpecSeq<letter_t>>>& curOutput = machineOutput[state];

        for (size_t a = 0; a < inputIDToSpecSeq.size(); a++) {
            state_id_t nextState = curNextState[a];
            if (nextState != NONE_STATE) {
                state_id_t mappedNextState = stateMap[nextState];
                SpecSeq<letter_t> input = SpecSeq<letter_t>(inputIDToSpecSeq[a]);
                std::vector<SpecSeq<letter_t>> output = curOutput[a];
                newMachine[curState].push_back(Transition(mappedNextState, input, output));
            }
        }
    }
}

}
