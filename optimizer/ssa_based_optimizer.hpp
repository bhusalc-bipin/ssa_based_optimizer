#pragma once

#include "analysis/dominance_analyzer.hpp"
#include "cfg/basic_block.hpp"
#include "cfg/cfg_generator.hpp"
#include "ssa/ssa_constructor.hpp"

#include <unordered_map>
#include <vector>

class SSA_Based_Optimizer {
private:
    std::unordered_map<int, DominanceInfo> dominance_info_;
    std::unordered_map<int, std::vector<int>> dom_tree_;
    SSA_Constructor ssa_constructor_;

    // Build the dominator tree from dominance info
    void build_dominator_tree(const CFG& cfg);

    // Mark unreachable blocks as deleted
    // NOTE: This is not a SSA based optimization and can be performed anytime after or during CFG
    // construction, but I wanted to put all optimization code in one place so putting it here.
    void eliminate_unreachable_blocks(const CFG& cfg, const std::vector<BasicBlock>& blocks,
        std::vector<Instruction>& instructions);

    // Dead code elimination (mark-sweep algorithm)
    void eliminate_useless_code(const CFG& cfg, const std::vector<BasicBlock>& blocks,
        std::vector<Instruction>& instructions);

public:
    void optimize(const CFG& cfg, const std::vector<BasicBlock>& blocks,
        std::vector<Instruction>& instructions);
};