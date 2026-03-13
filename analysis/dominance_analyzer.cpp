#include "./dominance_analyzer.hpp"

#include <unordered_set>

void Dominance_Analyzer::compute_dominators(const CFG& cfg) {
    const int entry_block = cfg.block_ids[0];

    // Initialization: the entry block is only dominated by itself, and all other blocks are
    // initially dominated by all blocks
    dominance_info[entry_block].dominators = { entry_block };

    std::unordered_set<int> all_blocks(cfg.block_ids.begin(), cfg.block_ids.end());
    for (size_t i = 1; i < cfg.block_ids.size(); i++) {
        dominance_info[cfg.block_ids[i]].dominators = all_blocks;
    }

    bool changed = true;
    while (changed) {
        changed = false;
        // skip the entry block (index 0) since it has no predecessors
        for (size_t i = 1; i < cfg.block_ids.size(); i++) {
            int current_block = cfg.block_ids[i];

            if (!cfg.predecessors.contains(current_block)) {
                continue; // unreachable block so skip it
            }
            const auto& predecessors = cfg.predecessors.at(current_block);

            std::unordered_set<int> new_dominators;
            // find the intersection of dominators of all predecessors
            for (const auto& pred : predecessors) {
                const auto& predecessor_dominators = dominance_info[pred].dominators;
                if (new_dominators.empty()) {
                    new_dominators = predecessor_dominators;
                } else {
                    std::erase_if(
                        new_dominators, [&](int b) { return !predecessor_dominators.contains(b); });
                }
            }

            new_dominators.insert(current_block); // a block always dominates itself

            if (new_dominators != dominance_info[current_block].dominators) {
                dominance_info[current_block].dominators = new_dominators;
                changed = true;
            }
        }
    }
}

/* Note: this function assumes that the dominators for each block have already been computed and
 * stored in the dominance_info map
 */
void Dominance_Analyzer::compute_immediate_dominators(const CFG& cfg) {
    const int entry_block = cfg.block_ids[0];
    dominance_info[entry_block].immediate_dominator = -1; // entry block has no immediate dominator

    // Amonng the strict dominators of a block, choose the one with the largest dominator set as the
    // immediate dominator.
    for (size_t i = 1; i < cfg.block_ids.size(); i++) {
        int current_block = cfg.block_ids[i];

        if (!cfg.predecessors.contains(current_block)) {
            continue; // skip unreachbale blocks
        }

        const auto& dominators = dominance_info[current_block].dominators;

        int best_candidate = -1;
        size_t current_max_size = 0;

        for (const auto& candidate : dominators) {
            if (candidate == current_block) {
                // skip itself because we only consider strict dominators for immediate dominator
                continue;
            }
            const auto& candidate_dominators = dominance_info[candidate].dominators;
            if (candidate_dominators.size() > current_max_size) {
                best_candidate = candidate;
                current_max_size = candidate_dominators.size();
            }
        }
        dominance_info[current_block].immediate_dominator = best_candidate;
    }
}

/* Note: this function assumes that the immediate dominators for each block have already been
 * computed and stored in the dominance_info map.
 */
void Dominance_Analyzer::compute_dominance_frontiers(const CFG& cfg) {
    // for each block in CFG set DF(n) to empty
    for (const auto& current_block : cfg.block_ids) {
        dominance_info[current_block].dominance_frontier = {};
    }

    for (const auto& current_block : cfg.block_ids) {
        if (!cfg.predecessors.contains(current_block)) {
            continue; // skip unreachbale blocks
        }

        if (cfg.predecessors.at(current_block).size() >= 2) {
            for (const auto& predecessor : cfg.predecessors.at(current_block)) {
                auto runner = predecessor;
                while (runner != dominance_info[current_block].immediate_dominator) {
                    dominance_info[runner].dominance_frontier.insert(current_block);
                    runner = dominance_info[runner].immediate_dominator;
                }
            }
        }
    }
}

void Dominance_Analyzer::build_dominator_tree(const CFG& cfg) {
    for (int block_id : cfg.block_ids) {
        if (!dominance_info.contains(block_id)) {
            continue;
        }
        int idom = dominance_info.at(block_id).immediate_dominator;
        if (idom != -1) {
            dominance_tree[idom].push_back(block_id);
        }
    }
}

void Dominance_Analyzer::perform_dominance_analysis(const CFG& cfg) {
    // order of these computations matters and the following is the correct order
    compute_dominators(cfg);
    compute_immediate_dominators(cfg);
    compute_dominance_frontiers(cfg);
    build_dominator_tree(cfg);
}