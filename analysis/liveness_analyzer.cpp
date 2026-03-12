#include "liveness_analyzer.hpp"
#include "opcode_info.hpp"

static bool is_register(const std::string& name) {
    return name.starts_with("%vr");
}

void Liveness_Analyzer::compute_local_information(const CFG& cfg,
    const std::vector<BasicBlock>& blocks, const std::vector<Instruction>& instructions) {

    for (int block_id : cfg.block_ids) {
        const auto& block = blocks[block_id];
        auto& ue = ue_vars[block_id];
        auto& kill = var_kill[block_id];

        for (int i = block.start_idx; i <= block.end_idx; i++) {
            const auto& instr = instructions[i];

            for (const auto& src : instr.source) {
                if (is_register(src) && !kill.contains(src)) {
                    ue.insert(src);
                }
            }

            if (OPCODES_USING_TARGET.contains(instr.opcode)) {
                // target is a memory address (a use), not a definition
                for (const auto& tgt : instr.target) {
                    if (is_register(tgt) && !kill.contains(tgt)) {
                        ue.insert(tgt);
                    }
                }
            } else {
                for (const auto& tgt : instr.target) {
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
        for (int block_id : cfg.block_ids) {
            std::unordered_set<std::string> new_live_out;
            if (cfg.successors.contains(block_id)) {
                for (int succ : cfg.successors.at(block_id)) {
                    if (succ == EXIT_BLOCK_ID)
                        continue;
                    for (const auto& var : liveness_info[succ].live_in) {
                        new_live_out.insert(var);
                    }
                }
            }

            if (new_live_out != liveness_info[block_id].live_out) {
                liveness_info[block_id].live_out = new_live_out;
                liveness_info[block_id].live_in = ue_vars.at(block_id);
                for (const auto& var : new_live_out) {
                    if (!var_kill.at(block_id).contains(var)) {
                        liveness_info[block_id].live_in.insert(var);
                    }
                }
                changed = true;
            }
        }
    }
}

void Liveness_Analyzer::perform_liveness_analysis(const CFG& cfg,
    const std::vector<BasicBlock>& blocks, const std::vector<Instruction>& instructions) {
    compute_local_information(cfg, blocks, instructions);
    solve_dataflow_equations(cfg);
}