/*
 * MeMin.h
 *
 *  Created on: 07.04.2015
 *      Author: Andreas Abel
 *  Modified on: 17.05.2018
 *      Author: Philipp Meyer
 */
#pragma once

#include <vector>
#include <queue>

#include "Machine.h"
#include "util/SpecSeq.h"

namespace mealy {

/*
 * Minimizes the Mealy machine given by 'machine'.
 * Assumes that all states are reachable from 'resetState.
 * If the machine could be minimized, 1 is returned
 * and the minimized machine is written to 'minimizedMachine'
 * The reset state of 'minimizedMachine' is always 0.
 * If the machine could not be minimized, 0 is returned.
 * If an error occurred, -1 is returned.
 */
int minimizeMachineMeMin(const machine_t& machine, state_id_t resetState, int numInputBits, int numOutputBits, machine_t& minimizedMachine, std::vector<std::vector<state_id_t> >& newStates, const int verbosity = 0);

}
