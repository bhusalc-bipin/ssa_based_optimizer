#include "ssa_based_optimizer.hpp"

#include <queue>

void SSA_Based_Optimizer::build_dominator_tree(const CFG& cfg) {
    dom_tree_.clear();
    for (int block_id : cfg.block_ids) {
        if (!dominance_info_.contains(block_id)) {
            continue;
        }
        int idom = dominance_info_.at(block_id).immediate_dominator;
        if (idom != -1) {
            dom_tree_[idom].push_back(block_id);
        }
    }
}

void SSA_Based_Optimizer::eliminate_unreachable_blocks(
    const CFG& cfg, const std::vector<BasicBlock>& blocks, std::vector<Instruction>& instructions) {
    // BFS from entry block to find all reachable blocks
    std::unordered_set<int> reachable;
    std::queue<int> worklist;
    reachable.insert(cfg.block_ids[0]);
    worklist.push(cfg.block_ids[0]);

    while (!worklist.empty()) {
        int current_block = worklist.front();
        worklist.pop();

        if (!cfg.successors.contains(current_block)) {
            continue;
        }
        for (int successor : cfg.successors.at(current_block)) {
            if (successor == EXIT_BLOCK_ID) {
                continue;
            }
            if (!reachable.contains(successor)) {
                reachable.insert(successor);
                worklist.push(successor);
            }
        }
    }

    // Mark instructions in unreachable blocks as deleted
    for (int block_id : cfg.block_ids) {
        if (reachable.contains(block_id)) {
            continue;
        }
        const auto& current_block = blocks[block_id];
        for (int i = current_block.start_idx; i <= current_block.end_idx; i++) {
            instructions[i].deleted = true;
        }
    }
}

void SSA_Based_Optimizer::perform_dbre(
    const CFG& cfg, const std::vector<BasicBlock>& blocks, std::vector<Instruction>& instructions) {
    // TODO: implement
}

void SSA_Based_Optimizer::eliminate_useless_code(
    const CFG& cfg, const std::vector<BasicBlock>& blocks, std::vector<Instruction>& instructions) {
    // TODO: implement
}

void SSA_Based_Optimizer::optimize(
    const CFG& cfg, const std::vector<BasicBlock>& blocks, std::vector<Instruction>& instructions) {

    // Dominance analysis
    Dominance_Analyzer dominance_analyzer;
    dominance_analyzer.perform_dominance_analysis(cfg);
    dominance_info_ = dominance_analyzer.dominance_info;
    build_dominator_tree(cfg);

    // Construct SSA form
    ssa_constructor_.construct_ssa_form(cfg, blocks, instructions, dominance_info_);

    // Optimize
    eliminate_unreachable_blocks(cfg, blocks, instructions);
    // DBRE is used before dependence-based optimization and other SSA-based optimizations
    perform_dbre(cfg, blocks, instructions);
    eliminate_useless_code(cfg, blocks, instructions);
}