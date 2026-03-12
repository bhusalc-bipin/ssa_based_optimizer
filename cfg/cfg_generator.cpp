#include "cfg/cfg_generator.hpp"
#include "opcode_info.hpp"

#include <unordered_map>

// Helper function to add edge from "from" block to "to" block in the CFG
static void add_edge(CFG& cfg, int from, int to) {
    cfg.successors[from].push_back(to);
    cfg.predecessors[to].push_back(from);
}

void CFG_Generator::build_cfg(
    std::vector<BasicBlock>& blocks, std::vector<Instruction>& instructions) {

    // Track which block belongs to which procedure (procedure name to list of block ids)
    // Using map from string to VECTOR so that the order of blocks is preserved
    std::unordered_map<std::string, std::vector<int>> procedure_to_blocks;
    // Track which label starts which block (label name to block id)
    std::unordered_map<std::string, int> label_to_block_id;

    for (const auto& block : blocks) {
        procedure_to_blocks[block.procedure_name].push_back(block.id);
        const auto& first_instr = instructions[block.start_idx];
        if (!first_instr.label.empty()) {
            label_to_block_id[first_instr.label] = block.id;
        }
    }

    // Create CFG for each procedure
    for (const auto& [procedure_name, block_ids] : procedure_to_blocks) {
        CFG cfg;
        cfg.procedure_name = procedure_name;
        cfg.block_ids = block_ids;

        // Edge from entry block (dummy block represented with id "-1") to the first real block
        // of the procedure
        add_edge(cfg, ENTRY_BLOCK_ID, block_ids[0]);

        // Add edges for each real block
        for (size_t i = 0; i < block_ids.size(); i++) {
            int current_block = block_ids[i];
            const auto& last_instruction = instructions[blocks[current_block].end_idx];
            const std::string& opcode = last_instruction.opcode;

            // edge to exit block (dummy block represented with id "-2")
            if (RETURN_OPCODES.contains(opcode)) {
                add_edge(cfg, current_block, EXIT_BLOCK_ID);
                continue;
            }

            // unconditional branch (except calls: handled below and returns: handled above) -> edge
            // to the target block
            if (UNCONDITIONAL_BRANCH_OPCODES_MINUS_CALLS_AND_RETURNS.contains(opcode)) {
                const std::string& target_label = last_instruction.target[0];
                int target_block_id = label_to_block_id[target_label];
                add_edge(cfg, current_block, target_block_id);
                continue;
            }

            // conditional branch -> two edges, one to the target block and one to the fallthrough
            // block
            if (CONDITIONAL_BRANCH_OPCODES.contains(opcode)) {
                // target block
                const std::string& target_label = last_instruction.target[0];
                int target_block_id = label_to_block_id[target_label];
                add_edge(cfg, current_block, target_block_id);

                // fallthrough block
                if (i + 1 < block_ids.size()) {
                    int fallthrough_block_id = block_ids[i + 1];
                    add_edge(cfg, current_block, fallthrough_block_id);
                }
                continue;
            }

            // in all other case there is no branch so add edge to the next block in the same
            // procedure (if exists)
            if (i + 1 < block_ids.size()) {
                int next_block_id = block_ids[i + 1];
                add_edge(cfg, current_block, next_block_id);
            }
        }
        cfgs_.push_back(cfg);
    }
}