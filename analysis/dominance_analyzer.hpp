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
private:
    void compute_dominators(const CFG& cfg);
    void compute_immediate_dominators(const CFG& cfg);
    void compute_dominance_frontiers(const CFG& cfg);
    void build_dominator_tree(const CFG& cfg);

public:
    // map from block id to its dominance info
    std::unordered_map<int, DominanceInfo> dominance_info;

    // parent block id -> list of children block ids
    std::unordered_map<int, std::vector<int>> dominance_tree;

    // Compute the immediate dominator, dominance frontier, and the list of dominators for each
    // block in the CFG and populate the dominance_info map accordingly.
    void perform_dominance_analysis(const CFG& cfg);
};