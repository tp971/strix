#pragma once

#include <vector>
#include <deque>
#include <map>
#include <set>
#include <memory>
#include <limits>
#include <iostream>

#include "owl.h"

#include "Definitions.h"
#include "Automaton.h"

namespace aut {

typedef product_state_t::const_iterator state_iter;

class ParityAutomatonTree {
    protected:
        size_t state_index;
        size_t state_width;

        ParityAutomatonTree(const NodeType node_type, const Parity parity_type, const color_t max_color) :
            state_index(0), node_type(node_type), parity_type(parity_type), max_color(max_color)
        {}

    public:
        virtual ~ParityAutomatonTree() {}

        const NodeType node_type;
        const Parity parity_type;
        const color_t max_color;

        virtual void getInitialState(product_state_t& state) = 0;
        virtual ColorScore getSuccessor(const product_state_t& state, product_state_t& new_state, letter_t letter) = 0;

        virtual void setState(product_state_t& new_state, node_id_t state) = 0;
        virtual void setTopState(product_state_t& new_state) = 0;
        virtual void setBottomState(product_state_t& new_state) = 0;

        virtual bool isTopState(const product_state_t& state) const = 0;
        virtual bool isBottomState(const product_state_t& state) const = 0;

        inline size_t getStateIndex() const { return state_index; }

        virtual int getMinIndex() const = 0;
        virtual letter_t getMaximumAlphabetSize() const = 0;
        virtual std::set<letter_t> getAlphabet() const = 0;

        inline void print_type() const {
            switch(node_type) {
                case NodeType::PARITY:
                    std::cout << "Parity " << ((parity_type == Parity::EVEN) ? "even" : "odd");
                    break;
                case NodeType::BUCHI:    std::cout << "Büchi";    break;
                case NodeType::CO_BUCHI: std::cout << "co-Büchi"; break;
                case NodeType::WEAK:     std::cout << "Weak";     break;
            }
        }
        virtual void print(const int verbosity = 0, const int indent = 0) const = 0;
};

class ParityAutomatonTreeLeaf : public ParityAutomatonTree {
    friend class AutomatonTreeStructure;

    private:
        Automaton& automaton;
        const owl::Reference reference;

    protected:
        ParityAutomatonTreeLeaf(
            Automaton& automaton,
            const owl::Reference reference
        );

    public:
        virtual ~ParityAutomatonTreeLeaf();

        void getInitialState(product_state_t& state) override;
        ColorScore getSuccessor(const product_state_t& state, product_state_t& new_state, letter_t letter) override;

        void setState(product_state_t& new_state, node_id_t state) override;
        void setTopState(product_state_t& new_state) override;
        void setBottomState(product_state_t& new_state) override;

        bool isTopState(const product_state_t& state) const override;
        bool isBottomState(const product_state_t& state) const override;

        virtual int getMinIndex() const override;
        virtual letter_t getMaximumAlphabetSize() const override;
        virtual std::set<letter_t> getAlphabet() const override;

        void print(const int verbosity = 0, const int indent = 0) const override;
};

class ParityAutomatonTreeNode : public ParityAutomatonTree {
    friend class AutomatonTreeStructure;

    protected:
        const owl::Tag tag;
        const std::vector<std::unique_ptr<ParityAutomatonTree>> children;
        const bool parity_child;
        const color_t dp;
        const node_id_t round_robin_size;

        ParityAutomatonTreeNode(
            const owl::Tag tag, const NodeType node_type, const Parity parity_type, const color_t max_color,
            const node_id_t round_robin_size, const bool parity_child,
            const color_t dp, std::vector<std::unique_ptr<ParityAutomatonTree>> children
        );

    public:
        virtual ~ParityAutomatonTreeNode();

        virtual void getInitialState(product_state_t& state) override;
        virtual ColorScore getSuccessor(const product_state_t& state, product_state_t& new_state, letter_t letter) override;

        virtual void setState(product_state_t& new_state, node_id_t state) override;
        virtual void setTopState(product_state_t& new_state) override;
        virtual void setBottomState(product_state_t& new_state) override;

        virtual bool isTopState(const product_state_t& state) const override;
        virtual bool isBottomState(const product_state_t& state) const override;

        virtual int getMinIndex() const override;
        virtual letter_t getMaximumAlphabetSize() const override;
        virtual std::set<letter_t> getAlphabet() const override;

        void print(const int verbosity = 0, const int indent = 0) const override;
};

class ParityAutomatonTreeBiconditionalNode : public ParityAutomatonTreeNode {
    friend class AutomatonTreeStructure;

    private:
        const int parity_child_index;
        const color_t d1;
        const color_t d2;

    protected:
        ParityAutomatonTreeBiconditionalNode(
            const NodeType node_type, const Parity parity_type, const color_t max_color,
            const node_id_t round_robin_size, const bool parity_child, const color_t dp,
            const int parity_child_index,
            std::vector<std::unique_ptr<ParityAutomatonTree>> children
        );

    public:
        virtual ~ParityAutomatonTreeBiconditionalNode();

        void getInitialState(product_state_t& state) override;
        ColorScore getSuccessor(const product_state_t& state, product_state_t& new_state, letter_t letter) override;

        void setState(product_state_t& new_state, node_id_t state) override;
        void setTopState(product_state_t& new_state) override;
        void setBottomState(product_state_t& new_state) override;

        bool isTopState(const product_state_t& state) const override;
        bool isBottomState(const product_state_t& state) const override;
};

class AutomatonTreeStructure {
    private:
        owl::DecomposedDPA owl_automaton;
        std::deque<Automaton> automata;
        std::unique_ptr<ParityAutomatonTree> tree;

        std::unique_ptr<ParityAutomatonTree> constructTree(const std::unique_ptr<owl::LabelledTree<owl::Tag, owl::Reference>>& tree, std::vector<ParityAutomatonTreeLeaf*>& leaves);
        std::vector<ParityAutomatonTreeLeaf*> leaves;

        std::vector<size_t> leaf_state_indices;
        product_state_t initial_state;

    public:
        AutomatonTreeStructure(owl::DecomposedDPA automaton);
        ~AutomatonTreeStructure();

        Parity getParityType() const;
        color_t getMaxColor() const;

        product_state_t getInitialState() const;
        ColorScore getSuccessor(const product_state_t& state, product_state_t& new_state, letter_t letter);

        std::vector<jint> getAutomatonStates(const product_state_t& state) const;
        bool declareWinning(const product_state_t& state, const Player winner);
        Player queryWinner(const product_state_t& state);

        bool isTopState(const product_state_t& state) const;
        bool isBottomState(const product_state_t& state) const;

        std::set<letter_t> getAlphabet() const;
        std::vector<owl::VariableStatus> getVariableStatuses() const;

        void clear();

        void print(const int verbosity = 0) const;
        void print_memory_usage() const;
};

}
