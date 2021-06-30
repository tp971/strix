/*
 * SpecSeq.h
 *
 * Authors:
 *  Philipp Meyer (original part from Strix)
 *  Andreas Abel (original part from MeMin)
 */
#pragma once

#include <iostream>
#include <vector>
#include <set>
#include <bitset>
#include <type_traits>
#include <limits>

#include "cuddObj.hh"
#include <boost/functional/hash.hpp>

// count the number of set bits in a number
template <typename I>
inline
typename std::enable_if<std::is_integral<I>::value, unsigned >::type
popcount(I number) {
    static_assert(std::numeric_limits<I>::radix == 2, "non-binary type");

    constexpr int bitwidth = std::numeric_limits<I>::digits + std::numeric_limits<I>::is_signed;
    static_assert(bitwidth <= std::numeric_limits<unsigned long long>::digits, "arg too wide for std::bitset() constructor");

    typedef typename std::make_unsigned<I>::type UI;

    std::bitset<bitwidth> bs( static_cast<UI>(number) );
    return bs.count();
}

template <typename I>
struct SpecSeq {
    //the i-th bit is 1 iff the i-th element is specified and 1, otherwise the bit is 0
    I number;

    //the i-th bit is 1 iff the i-th element is not specified
    I unspecifiedBits;

    SpecSeq() : number(0), unspecifiedBits(0) {}
    SpecSeq(I n) : number(n), unspecifiedBits(0) {}
    SpecSeq(I n, I u) : number(n), unspecifiedBits(u) {}

    //bits that are specified in both outputs must be the same
    bool isCompatible(const SpecSeq<I>& other) const {
        const I mask = unspecifiedBits | other.unspecifiedBits;
        return (number | mask) == (other.number | mask);
    }

    //bits are specified if they are specified in at least one of the sequences
    //assumes that sequences have the same length and that they are compatible
    SpecSeq<I> intersect(const SpecSeq<I>& other) const {
        return SpecSeq<I>(number | other.number, unspecifiedBits & other.unspecifiedBits);
    }

    //two sequences are disjoint if their intersection is empty
    bool isDisjoint(const SpecSeq<I>& other) const {
        const I mask = unspecifiedBits | other.unspecifiedBits;
        return (number | mask) != (other.number | mask);
    }

    //all words that are in this sequence, but not in the other
    //assumes that there is at least one such word
    std::vector<SpecSeq<I>> diff(const SpecSeq<I>& other, int numBits) const {
        std::vector<SpecSeq<I>> ret;

        SpecSeq<I> last = *this;
        for (int b = 0; b < numBits; b++) {
            I bit = ((I)1 << b);
            if ((unspecifiedBits & bit) != 0 && (other.unspecifiedBits & bit) == 0) {
                SpecSeq<I> newSeq = last;

                newSeq.unspecifiedBits &= ~bit;
                last.unspecifiedBits &= ~bit;

                newSeq.number |= ((~other.number) & bit);
                last.number |= (other.number & bit);

                ret.push_back(newSeq);
            }
        }

        return ret;
    }

    //if a bit of this seq is unspecified, the corresponding bit of the other seq must also be unspecified
    //all specified bits must be equal
    bool isSubset(const SpecSeq<I>& other) const {
        return ((unspecifiedBits | other.unspecifiedBits) == other.unspecifiedBits) && isCompatible(other);
    }

    bool isFullySpecified() const {
        return unspecifiedBits == 0;
    }

    bool operator<(const SpecSeq<I>& other) const {
        return number < other.number || (number == other.number && unspecifiedBits < other.unspecifiedBits);
    }

    bool operator==(const SpecSeq<I>& other) const {
        return number == other.number && unspecifiedBits == other.unspecifiedBits;
    }

    bool operator!=(const SpecSeq<I>& other) const {
        return !operator==(other);
    }

    BDD toBDD(const Cudd& manager, const int numBits) {
        constexpr int bitwidth = std::numeric_limits<I>::digits + std::numeric_limits<I>::is_signed;
        int array[bitwidth];
        for (int b = 0; b < numBits; b++) {
            const I bit = ((I)1 << b);
            if ((unspecifiedBits & bit) == 0) {
                array[b] = (number & bit) >> b;
            }
            else {
                array[b] = -1;
            }
        }
        return BDD(manager, Cudd_CubeArrayToBdd(manager.getManager(), array));
    }

    std::string toString(const int numBits) const {
        std::stringstream s;
        for (I val = 1; val < ((I)1 << numBits); val <<= 1) {
            if ((unspecifiedBits & val) == 0) {
                if ((number & val) == 0) {
                    s << "0";
                }
                else {
                    s << "1";
                }
            }
            else {
                s << "-";
            }
        }
        return s.str();
    }

    std::string toVectorString(const std::vector<int>& accumulatedBits) const {
        std::stringstream s;
        s << "(";
        bool empty = true;
        for (size_t i = 0; i + 1 < accumulatedBits.size(); i++) {
            int startBit = accumulatedBits[i];
            int endBit = accumulatedBits[i+1];
            int numBits = endBit - startBit;
            if (numBits > 0) {
                if (!empty) {
                    s << ",";
                }
                else {
                    empty = false;
                }

                I mask = (((I)1 << numBits) - 1);
                I num = (number >> startBit) & mask;
                I unspec = (unspecifiedBits >> startBit) & mask;
                if (unspec != mask) {
                    s << num;
                }
                else {
                    s << "-";
                }
            }
        }
        s << ")";
        return s.str();
    }

    std::string toStateLabelledString(const std::vector<int>& accumulatedBits) const {
        std::stringstream s;
        int totalBits = accumulatedBits.back();
        if (unspecifiedBits == (((I)1 << totalBits) - 1)) {
            // add top/true
            s << "&#8868;";
        }
        else {
            bool empty = true;
            for (size_t i = 0; i + 1 < accumulatedBits.size(); i++) {
                int startBit = accumulatedBits[i];
                int endBit = accumulatedBits[i+1];
                int numBits = endBit - startBit;
                if (numBits > 0) {
                    I mask = (((I)1 << numBits) - 1);
                    I num = (number >> startBit) & mask;
                    I unspec = (unspecifiedBits >> startBit) & mask;
                    if (unspec != mask) {
                        if (!empty) {
                            // add logical and
                            s << "&#8743;";
                        }
                        else {
                            empty = false;
                        }
                        s << "s";
                        // add index using Unicode subscript numbers
                        int index = i;
                        std::vector<int> digits;
                        if (index == 0) {
                            digits.push_back(0);
                        }
                        else {
                            while (index > 0) {
                                digits.push_back(index % 10);
                                index /= 10;
                            }
                        }
                        for (auto it = digits.rbegin(); it != digits.rend(); it++) {
                            s << "&#832" << *it << ";";
                        }
                        s << "=" << num;
                    }
                }
            }
        }
        return s.str();
    }

    std::string toLabelledString(const int numBits, const std::vector<std::string>& bitNames) const {
        std::stringstream s;
        if (unspecifiedBits == (((I)1 << numBits) - 1)) {
            // add top/true
            s << "&#8868;";
        }
        else {
            bool empty = true;
            for (int bit = 0; bit < numBits; bit++) {
                I val = ((I)1 << bit);

                if ((unspecifiedBits & val) == 0) {
                    if (!empty) {
                        // add logical and
                        s << "&#8743;";
                    }
                    else {
                        empty = false;
                    }
                    if ((number & val) == 0) {
                        // add logical negation
                        s << "&#172;";
                    }
                    s << bitNames[bit];
                }
            }
        }
        return s.str();
    }

    friend std::ostream &operator<<(std::ostream &out, const SpecSeq<I>& output) {
        std::vector<int> bits;
        constexpr int num_bits = 8;
        int count = 0;
        I n = output.number;
        I d = output.unspecifiedBits;

        while (n > 0 || count < num_bits) {
            if (d%2 == 0) {
                bits.push_back(n%2);
            }
            else {
                bits.push_back(2);
            }

            n >>= 1;
            d >>= 1;
            count++;
        }

        for (auto bit = bits.rbegin(); bit != bits.rend(); bit++) {
            if (*bit == 2) {
                out << "-";
            }
            else {
                out << *bit;
            }
        }
        return out;
    }
};

template <typename I>
struct std::hash<SpecSeq<I>> {
    std::size_t operator()(const SpecSeq<I>& val) const {
        std::size_t seed = 0;
        boost::hash_combine(seed, val.number);
        boost::hash_combine(seed, val.unspecifiedBits);
        return seed;
    }
};

template <typename I>
SpecSeq<I> true_clause(const int numBits) {
    return SpecSeq<I>(0, ((I)1 << numBits) - 1);
}

template <typename I>
class BDDtoSpecSeqIterator {
private:
    BDD bdd;
    const int numBits;
    bool done_;

    // current cube
    DdGen* gen;
    int* cube;
    CUDD_VALUE_TYPE value;
    SpecSeq<I> current;

    void update() {
        if (Cudd_IsGenEmpty(gen)) {
            done_ = true;
        }
        else {
            // create SpecSeq
            current = SpecSeq<I>();

            for (int i = 0; i < numBits; i++) {
                const I b = ((I)1 << i);
                switch (cube[i]) {
                    case 0:
                        // bit not set
                        break;
                    case 1:
                        // bit set
                        current.number |= b;
                        break;
                    case 2:
                        // unspecified bit
                        current.unspecifiedBits |= b;
                        break;
                }
            }
        }
    }

public:
    BDDtoSpecSeqIterator(BDD bdd, const int numBits) :
        bdd(bdd), numBits(numBits), done_(false)
    {
        // initialize gen and grab first cube
        gen = Cudd_FirstCube(bdd.manager(), bdd.getNode(), &cube, &value);
        update();
    }

    ~BDDtoSpecSeqIterator() {
        Cudd_GenFree(gen);
    }

    inline void operator++() {
        // grab next cube
        Cudd_NextCube(gen, &cube, &value);
        update();
    }
    inline void operator++(int) {
        operator++();
    }

    inline const SpecSeq<I> operator*() const {
        return current;
    }

    inline bool done() const {
        return done_;
    }
};
