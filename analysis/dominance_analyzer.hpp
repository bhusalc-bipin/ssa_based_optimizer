#pragma once

#include "cfg/cfg_generator.hpp"

#include <unordered_map>
#include <unordered_set>

struct DominanceInfo {
    int immediate_dominator = -1; // -1 to indicate no immediate dominator (e.g., for entry block)
    std::unordered_set<int> dominance_frontier;
    std::unordered_set<int> dominators;
};

class Dominance_Analyzer {
public:
    // map from block id to its dominance info
    std::unordered_map<int, DominanceInfo> dominance_info;

    // Compute the immediate dominator, dominance frontier, and the list of dominators for each
    // block in the CFG and populate the dominance_info map accordingly.
    void perform_dominance_analysis(const CFG& cfg);
};