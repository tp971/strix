/*
 * MachineBuilder.h
 *
 *  Created on: 06.03.2015
 *      Author: Andreas Abel
 *  Modified on: 17.05.2018
 *      Author: Philipp Meyer
 */
#pragma once

#include "Machine.h"
#include "DIMACSWriter.h"
#include "util/SpecSeq.h"

namespace mealy {

bool buildMachine(std::vector<std::vector<state_id_t> >& newMachineNextState, std::vector<std::vector<std::vector<SpecSeq<letter_t>>> >& newMachineOutput, state_id_t& newResetState, std::vector<std::vector<state_id_t> >& newStates, state_id_t nClasses, std::vector<DIMACSWriter::lit_t>& dimacsOutput, std::vector<std::pair<state_id_t, state_id_t> >& literalToStateClass, std::vector<std::vector<state_id_t> >& origMachineNextState, std::vector<std::vector<std::vector<SpecSeq<letter_t>>> >& origMachineOutput, state_id_t origResetState, size_t numInputs, const int verbosity = 0);

void constructMachine(std::vector<std::vector<state_id_t> >& machineNextState, std::vector<std::vector<std::vector<SpecSeq<letter_t>>> >& machineOutput, state_id_t resetState, std::vector<std::vector<state_id_t> >& stateMapping, std::vector<SpecSeq<letter_t>>& inputIDToSpecSeq, machine_t& newMachine);

}
