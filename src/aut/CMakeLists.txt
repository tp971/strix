set (aut_SRCS Automaton.cc ParityAutomatonTreeStructure.cc ParityAutomatonTreeLeaf.cc ParityAutomatonTreeNode.cc ParityAutomatonTreeBiconditionalNode.cc)

set (TARGET "aut")

add_library(${TARGET} STATIC ${aut_SRCS})

target_link_libraries (${TARGET} ltl)
target_link_libraries (${TARGET} owl)
