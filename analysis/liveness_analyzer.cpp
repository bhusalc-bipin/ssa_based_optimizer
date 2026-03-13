#include "liveness_analyzer.hpp"
#include "opcode_info.hpp"

static bool is_register(const std::string& name) {
    return name.starts_with("%vr");
}

void Liveness_Analyzer::compute_local_information(const CFG& cfg,
    const std::vector<BasicBlock>& blocks, const std::vector<Instruction>& instructions) {

    for (int block_id : cfg.block_ids) {
        const auto& current_block = blocks[block_id];
        auto& ue = ue_vars[block_id];
        auto& kill = var_kill[block_id];

        for (int i = current_block.start_idx; i <= current_block.end_idx; i++) {
            const auto& current_instruction = instructions[i];

            for (const auto& src : current_instruction.source) {
                if (is_register(src) && !kill.contains(src)) {
                    ue.insert(src);
                }
            }

            if (OPCODES_USING_TARGET.contains(current_instruction.opcode)) {
                // target is a use (memory address), not definition.
                // iread and fread (due to parser design) have their target as source operand, but
                // since target is use, it's already handled above when processing source operands.
                for (const auto& tgt : current_instruction.target) {
                    if (is_register(tgt) && !kill.contains(tgt)) {
                        ue.insert(tgt);
                    }
                }
            } else {
                for (const auto& tgt : current_instruction.target) {
                    if (is_register(tgt)) {
                        kill.insert(tgt);
                    }
                }
            }
        }
    }
}

void Liveness_Analyzer::solve_dataflow_equations(const CFG& cfg) {
    for (int block_id : cfg.block_ids) {
        liveness_info[block_id].live_out = {};
        liveness_info[block_id].live_in = ue_vars.at(block_id);
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (int current_block : cfg.block_ids) {
            // recompute live_out as union of live_in of successors
            std::unordered_set<std::string> new_live_out;
            if (cfg.successors.contains(current_block)) {
                for (int successor : cfg.successors.at(current_block)) {
                    if (successor == EXIT_BLOCK_ID)
                        continue;
                    for (const auto& var : liveness_info[successor].live_in) {
                        new_live_out.insert(var);
                    }
                }
            }

            // if live_out changed, update live_in and mark changed = true to continue iteration
            // until convergence
            if (new_live_out != liveness_info[current_block].live_out) {
                liveness_info[current_block].live_out = new_live_out;
                liveness_info[current_block].live_in = ue_vars.at(current_block);
                for (const auto& var : new_live_out) {
                    if (!var_kill.at(current_block).contains(var)) {
                        liveness_info[current_block].live_in.insert(var);
                    }
                }
                changed = true;
            }
        }
    }
}

void Liveness_Analyzer::perform_liveness_analysis(const CFG& cfg,
    const std::vector<BasicBlock>& blocks, const std::vector<Instruction>& instructions) {

    ue_vars.clear();
    var_kill.clear();
    liveness_info.clear();

    compute_local_information(cfg, blocks, instructions);
    solve_dataflow_equations(cfg);
}