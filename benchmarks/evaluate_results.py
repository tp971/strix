#!/usr/bin/env python3

import csv
import math
import sys

if len(sys.argv) < 2:
    print("Usage: %s RESULTSFILE" % sys.argv[0])
    sys.exit(1)

results_file = sys.argv[1]

# read results from SYNTCOMP
syntcomp = {}
with open('results_syntcomp2017.csv', 'r') as csvfile:
    reader = csv.DictReader(csvfile)
    for row in reader:
        syntcomp[row['file']] = row

tools = ['Bowser_c0', 'spot_ltlsynt', 'Party_bool', 'Bowser_c1', 'Bowser_c2', 'Party_kid_aiger', 'BoSy_ltl3ba', 'BoSy_spot', 'Acacia_4_Aiger', 'Party_int',
         'BoSy_ltl3ba_par', 'BoSy_spot_par', 'Bowser_c0_par', 'Bowser_c1_par', 'Bowser_c2_par', 'Acacia_4_Aiger_par', 'Party_portfolio']

realizable = 0
unrealizable = 0
realizability_solved = 0
realizability_unique = 0
aiger = 0
verified = 0
synthesis_solved = 0
synthesis_unique = 0
verification_timeouts = 0

total_improved = 0
total_minimized = 0
count_minimized = 0
total_amount_minimized = 0
minimization_decrease = 0
minimization_decrease_amount = 0
minimization_increase = 0
minimization_increase_amount = 0
total_quality = 0

def compute_quality(filename, size):
    reference = size
    best2017 = None
    for tool in tools:
        try:
            toolsize = int(syntcomp[filename]["size_synthesis_%s" % tool])
            if toolsize < reference:
                reference = toolsize
            if best2017 == None or toolsize < best2017:
                best2017 = toolsize
        except ValueError:
            pass
    try:
        # use best of 2016 as reference if available, otherwise best of 2017 if available, otherwise solution of tool
        best2016 = int(syntcomp[filename]['best_size_synthesis_2016'])
        reference = best2016
    except ValueError:
        pass
    improved = False
    unique = False
    if best2017 != None and size < best2017:
        improved = True
        relative = (best2017 - size) * 100 / best2017
        print("Better synthesis solution than existing minium on %s: size %s < %s (reduction by %.2f%%)" % (filename, size, best2017, relative))
    if best2017 == None:
        unique = True
        print("Unique verified synthesis solution for %s: size %s" % (filename, size))
    quality = max(0, 2 - math.log((size + 1) / (reference + 1), 10))
    return [quality, improved, unique]

def is_unique_realizability_result(filename, result):
    for tool in tools:
        if syntcomp[filename]["result_realizability_%s" % tool] == result:
            return False
    return True

with open(results_file, 'r') as csvfile:
    reader = csv.DictReader(csvfile)
    for row in reader:
        filename = row['file']
        result_realizability = row['result_realizability']
        classification = syntcomp[filename]['classification']
        if (result_realizability == 'realizable' or result_realizability == 'unrealizable') and result_realizability != classification:
            print("Wrong answer for %s: %s (should be %s)" % (filename, result_realizability, classification))
        elif result_realizability == 'realizable':
            realizable += 1
            realizability_solved += 1
            if is_unique_realizability_result(filename, result_realizability):
                realizability_unique += 1
                print("Unique realizability result for %s: %s" % (filename, result_realizability))
            try:
                size = int(row['result_synthesis'])
                aiger += 1
                if row['result_verification'] == 'passed':
                    verified += 1
                    synthesis_solved += 1
                    cq = compute_quality(filename, size)
                    quality = cq[0]
                    if cq[1]:
                        total_improved += 1
                    if cq[2]:
                        synthesis_unique += 1
                    total_quality += quality
                elif row['result_verification'] == 'unknown':
                    verification_timeouts += 1
                elif row['result_verification'] == 'failed':
                    print("Verification failed for %s" % filename)
            except ValueError:
                # synthesis not solved
                pass
            if 'size_mealy' in row:
                try:
                    # compute effect on minimization
                    size_mealy = int(row['size_mealy'])
                    size_min_mealy = int(row['size_min_mealy'])
                    if size_min_mealy < size_mealy:
                        minimized = (size_mealy - size_min_mealy) / size_mealy
                        total_amount_minimized += minimized
                        total_minimized += 1
                        try:
                            size_aiger = int(row['size_reduced_aiger'])
                            size_min_aiger = int(row['size_min_reduced_aiger'])
                            if size_min_aiger < size_aiger:
                                minimized = (size_aiger - size_min_aiger) / size_aiger
                                minimization_decrease_amount += minimized
                                print("Minimization decreases size for %s: %d < %d" % (filename, size_min_aiger, size_aiger))
                                minimization_decrease += 1
                            if size_aiger < size_min_aiger:
                                if size_aiger == 0:
                                    minimized = 1
                                else:
                                    minimized = (size_min_aiger - size_aiger) / size_aiger
                                print("Minimization increases size for %s: %d > %d" % (filename, size_min_aiger, size_aiger))
                                minimization_increase_amount += minimized
                                minimization_increase += 1
                        except ValueError:
                            pass
                except ValueError:
                    # minimization method failed
                    pass
        elif result_realizability == 'unrealizable':
            if is_unique_realizability_result(filename, result_realizability):
                realizability_unique += 1
                print("Unique realizability result for %s: %s" % (filename, result_realizability))
            unrealizable += 1
            realizability_solved += 1
            synthesis_solved += 1
            total_quality += 1

print()
print("Instances where realizability was determined: %i" % realizable)
print("Instances where unrealizability was determined : %i" % unrealizable)
print("Total solved for realizability: %i" % realizability_solved)
print("Unique instances for realizability: %i" % realizability_unique)
print("Instances where an AIGER circuit was produced: %i" % aiger)
print("Instances that could be verified for synthesis: %i" % verified)
print("Total solved for synthesis: %i" % synthesis_solved)
print("Unique instances for synthesis: %i" % synthesis_unique)
print("Quality rating: %f" % total_quality)
if synthesis_solved > 0:
    print("Average quality: %f" % (total_quality / synthesis_solved))
print("Improvements over best existing solution: %i" % total_improved)

print()
if total_minimized > 0:
    print("Instances where minimization reduces size of Mealy machine: %i (average reduction is by %.2f%%)" % (total_minimized, (total_amount_minimized * 100 / total_minimized)))
if minimization_decrease > 0:
    print("Instances where AIGER circuit size decreases with minimization: %i (on average by %.2f%%)" % (minimization_decrease, (minimization_decrease_amount * 100 / minimization_decrease)))
if minimization_increase > 0:
    print("Instances where AIGER circuit size increases with minimization: %i (on average by %.2f%%)" % (minimization_increase, (minimization_increase_amount * 100 / minimization_increase)))
