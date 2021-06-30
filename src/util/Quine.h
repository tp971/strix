#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <cassert>

#include "SpecSeq.h"

/*
 * Table of minterms and implicants
 * first dimension: number of unspecified bits
 * second dimension: number of ones
 * third dimension: value of unspecified bits
 * fourth dimension: value of number
 *
 * To combine terms:
 * - number of unspecified bits has to be equal
 * - number of ones have to differ by exactly one
 * - value of unspecified bits has to be equal
 * - combine function has to return true
 */
template<typename I>
struct quine_table {
    size_t size;
    std::vector< std::vector< std::unordered_map< I, std::unordered_map< I, bool > > > > cells;
};

// print the table
template<typename I>
void print_table(const quine_table<I>& table) {
    for (size_t s = 0; s < table.size; s++) {
        std::cout<<"\n-------------------------------------"<<std::endl;
        std::cout << "Size " << s << ":" << std::endl;
        for (size_t i = 0; i < table.cells[s].size(); i++) {
            if (table.cells[s][i].size() > 0) {
                std::cout << i;
            }
            for (const auto& esum_entry : table.cells[s][i]) {
                for (const auto& entry : esum_entry.second) {
                    std::cout << "\t\t" << entry.first << "(" << esum_entry.first << ")";
                    if (entry.second) {
                        std::cout << "*";
                    }
                    else {
                        std::cout << " ";
                    }
                    std::cout << std::endl;
                }
            }
        }
        std::cout<<"\n-------------------------------------"<<std::endl;
    }
}

template<typename I>
quine_table<I> compute_quine_table(const int num_bits, const std::vector<SpecSeq<I>>& values) {
    assert(num_bits > 0);
    assert(values.size() > 0);
    std::cout << "Computing Quine table" << std::endl;
    for (const auto val : values) {
        std::cout << val.toString(num_bits) << std::endl;
    }
    // TODO: make algorithm work with inputs such as {0-, 10, -1} to obtain {--}

    quine_table<I> table;
    table.size = 1;
    // resize table according to number of bits
    table.cells.resize(num_bits + 1);
    for (size_t size = 0; size < table.cells.size(); size++) {
        table.cells[size].resize(num_bits + 1);
    }

    // fill table with input values
    for (const auto val : values) {
        const size_t num_dontcares = popcount(val.unspecifiedBits);
        const size_t num_ones = popcount(val.number);

        table.cells[num_dontcares][num_ones][val.unspecifiedBits].insert({ val.number, true });
        table.size = std::max(table.size, num_dontcares + 1);
    }

    // compute implicants by increasing size
    bool new_value = true;
    size_t cur_row = 0;
    while (new_value || cur_row < table.size) {
        new_value = false;
        for (size_t i = 0; i+1 < table.cells[cur_row].size(); i++) {
            for (auto& lhs_dontcares_entry : table.cells[cur_row][i]) {
                auto rhs_dontcares_entry = table.cells[cur_row][i+1].find(lhs_dontcares_entry.first);
                if (rhs_dontcares_entry != table.cells[cur_row][i+1].end()) {
                    for (auto& lhs_number : lhs_dontcares_entry.second) {
                        for (auto& rhs_number : rhs_dontcares_entry->second) {
                            const I tmp_dontcares = lhs_number.first ^ rhs_number.first;
                            // test if tmp_dontcares is a power of two
                            if ((tmp_dontcares & (tmp_dontcares-1)) == 0) {
                                const I number = lhs_number.first & rhs_number.first;
                                const I dontcares = tmp_dontcares | lhs_dontcares_entry.first;
                                table.cells[cur_row + 1][i][dontcares].insert({ number, true });

                                lhs_number.second = false;
                                rhs_number.second = false;
                                new_value = true;
                            }
                        }
                    }
                }
            }
        }
        cur_row++;
    }
    table.size = cur_row;

    return table;
}

template<typename I>
std::vector<SpecSeq<I>> quine_prime_implicants(const int num_bits, const std::vector<SpecSeq<I>>& values) {
    if (values.size() == 1) {
        return { values[0] };
    }
    if (values.size() == ((size_t)1 << num_bits)) {
        // Assuming all values are distinct, all values can be combined.
        // Directly return minimal implicant for this number of bits.
        return { true_clause<I>(num_bits) };
    }

    std::vector<SpecSeq<I>> prime_implicants;

    quine_table<I> table = compute_quine_table(num_bits, values);

    for (size_t s = 0; s < table.size; s++) {
        for (size_t i = 0; i < table.cells[s].size(); i++) {
            for (const auto& dontcares_entry : table.cells[s][i]) {
                for (const auto& number_entry: dontcares_entry.second) {
                    if (number_entry.second) {
                        prime_implicants.push_back(SpecSeq<I>(number_entry.first, dontcares_entry.first));
                    }
                }
            }
        }
    }

    return prime_implicants;
}

template<typename I>
SpecSeq<I> quine_minimal_implicant(const int num_bits, const std::vector<SpecSeq<I>>& values) {
    if (values.size() == 1) {
        return values[0];
    }
    else if (values.size() == ((size_t)1 << num_bits)) {
        // Assuming all values are distinct, all values can be combined.
        // Directly return minimal implicant for this number of bits.
        return true_clause<I>(num_bits);
    }

    quine_table<I> table = compute_quine_table(num_bits, values);

    const size_t s = table.size - 1;
    for (size_t i = 0; i < table.cells[s].size(); i++) {
        for (const auto& dontcares_entry : table.cells[s][i]) {
            for (const auto& number_entry: dontcares_entry.second) {
                std::cout << "Returning minimal implicant " << SpecSeq<I>(number_entry.first, dontcares_entry.first).toString(num_bits) << std::endl;
                return SpecSeq<I>(number_entry.first, dontcares_entry.first);
            }
        }
    }

    throw std::invalid_argument("empty set of implicants");
}
