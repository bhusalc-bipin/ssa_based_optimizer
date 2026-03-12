#include "ssa_based_optimizer.hpp"

#include <queue>

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
    dominance_tree_ = dominance_analyzer.dominance_tree;

    // Construct SSA form
    ssa_constructor_.construct_ssa_form(cfg, blocks, instructions, dominance_info_);

    // Optimize
    eliminate_unreachable_blocks(cfg, blocks, instructions);
    eliminate_useless_code(cfg, blocks, instructions);
}