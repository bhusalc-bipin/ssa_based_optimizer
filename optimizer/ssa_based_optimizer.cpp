#include "ssa_based_optimizer.hpp"
#include "analysis/dominance_analyzer.hpp"
#include "opcode_info.hpp"

#include <queue>

static bool is_critical_instruction(const Instruction& instruction) {
    if (instruction.opcode == "jumpI" || OPCODES_WITH_SIDE_EFFECT.contains(instruction.opcode)) {
        return true;
    }
    return false;
}

static bool is_register(const std::string& name) {
    return name.starts_with("%vr");
}

// Helper function to track which variables are defined by which instruction
static void populate_definition_map(const CFG& cfg, const std::vector<BasicBlock>& blocks,
    const std::vector<Instruction>& instructions,
    std::unordered_map<std::string, int>& definition_map) {

    for (int block_id : cfg.block_ids) {
        const auto& current_block = blocks[block_id];
        for (int i = current_block.start_idx; i <= current_block.end_idx; i++) {
            const auto& current_instruction = instructions[i];

            if (current_instruction.deleted || current_instruction.target.empty()
                || OPCODES_USING_TARGET.contains(current_instruction.opcode)) {
                continue;
            }

            if (is_register(current_instruction.target[0])) {
                definition_map[current_instruction.target[0]] = i;
            }
        }
    }
}

// Helper function to track which instruction is in which block
static void populate_instruction_to_block_map(const CFG& cfg, const std::vector<BasicBlock>& blocks,
    std::unordered_map<int, int>& instruction_to_block_map) {

    for (int block_id : cfg.block_ids) {
        const auto& current_block = blocks[block_id];
        for (int i = current_block.start_idx; i <= current_block.end_idx; i++) {
            instruction_to_block_map[i] = block_id;
        }
    }
}

// Helper function to reverse the CFG
static void reverse_cfg(const CFG& cfg, CFG& reversed_cfg) {
    static const int REVERSE_ENTRY_BLOCK_ID = -3;

    reversed_cfg.block_ids.push_back(REVERSE_ENTRY_BLOCK_ID);
    for (int block_id : cfg.block_ids) {
        reversed_cfg.block_ids.push_back(block_id);
    }

    // reverse all edges
    for (int block_id : cfg.block_ids) {
        if (!cfg.successors.contains(block_id)) {
            continue;
        }
        for (int successor : cfg.successors.at(block_id)) {
            if (successor == EXIT_BLOCK_ID) {
                reversed_cfg.successors[REVERSE_ENTRY_BLOCK_ID].push_back(block_id);
                reversed_cfg.predecessors[block_id].push_back(REVERSE_ENTRY_BLOCK_ID);
            } else {
                reversed_cfg.successors[successor].push_back(block_id);
                reversed_cfg.predecessors[block_id].push_back(successor);
            }
        }
    }
}

// Helper function to find the nearest marked postdominator block of a given block
static int find_nearest_marked_postdominator(const std::vector<BasicBlock>& blocks,
    const std::vector<Instruction>& instructions,
    const std::unordered_map<int, DominanceInfo>& post_dominance_info,
    const std::unordered_set<int>& marked, int current_block_id) {

    // traverse up the postdominator tree using immediate_dominator
    while (current_block_id != -1) {
        // check if this block has any marked instruction
        if (current_block_id >= 0) { // skip dummy nodes
            const auto& current_block = blocks[current_block_id];
            for (int i = current_block.start_idx; i <= current_block.end_idx; i++) {
                if (!instructions[i].deleted && marked.contains(i)) {
                    return current_block_id;
                }
            }
        }
        // move up to immediate postdominator
        if (!post_dominance_info.contains(current_block_id)) {
            break;
        }
        current_block_id = post_dominance_info.at(current_block_id).immediate_dominator;
    }
    return current_block_id;
}

void SSA_Based_Optimizer::eliminate_unreachable_blocks(
    const CFG& cfg, const std::vector<BasicBlock>& blocks, std::vector<Instruction>& instructions) {

    // Rebuild fresh CFG from non-deleted instructions
    CFG_Generator fresh_cfg_generator;
    fresh_cfg_generator.build_cfg(const_cast<std::vector<BasicBlock>&>(blocks), instructions);

    // since cfg builder creates cfg for whole program, we need to first find the fresh cfg for this
    // procedure because this function is called for each procedure separately
    const CFG* fresh_cfg = nullptr;
    for (const auto& c : fresh_cfg_generator.cfgs_) {
        if (c.procedure_name == cfg.procedure_name) {
            fresh_cfg = &c;
            break;
        }
    }
    if (!fresh_cfg) {
        return;
    }

    // BFS from entry block to find all reachable blocks
    std::unordered_set<int> reachable;
    std::queue<int> worklist;
    reachable.insert(fresh_cfg->block_ids[0]);
    worklist.push(fresh_cfg->block_ids[0]);

    while (!worklist.empty()) {
        int current_block = worklist.front();
        worklist.pop();

        if (!fresh_cfg->successors.contains(current_block)) {
            continue;
        }
        for (int successor : fresh_cfg->successors.at(current_block)) {
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
    for (int block_id : fresh_cfg->block_ids) {
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

    // MARK PHASE

    std::queue<int> worklist;
    std::unordered_set<int> marked; // set of useful instruction indices

    for (int block_id : cfg.block_ids) {
        const auto& current_block = blocks[block_id];
        for (int i = current_block.start_idx; i <= current_block.end_idx; i++) {
            const auto& current_instruction = instructions[i];

            if (current_instruction.deleted) {
                continue;
            }
            if (is_critical_instruction(current_instruction)) {
                marked.insert(i);
                worklist.push(i);
            }
        }
    }

    // track which variable is defined by which instruction
    // map from variable name to instruction index
    std::unordered_map<std::string, int> definition_map;
    populate_definition_map(cfg, blocks, instructions, definition_map);

    // special value to indicate that the variable is defined by a phi node (used for handling phi
    // nodes in the marking phase)
    const int PHI_DEFINITION_INDICATOR = -99;
    // track which phi function maps to which variable (variable name to phi function)
    std::unordered_map<std::string, const PhiFunction*> phi_map;

    // NOTE: Need this special handling because I am not explicitly representing phi nodes as
    // instructions in the instructions vector, so they don't have index. But I still want to be
    // able to mark variables defined by phi nodes as useful when they are used by critical
    // instructions or other useful instructions. When I encounter this value in the marking phase,
    // I mark all the source variables of the phi node as useful.
    for (const auto& [block_id, phis] : ssa_constructor_.phi_functions) {
        for (const auto& phi : phis) {
            definition_map[phi.target] = PHI_DEFINITION_INDICATOR;
            phi_map[phi.target] = &phi;
        }
    }

    // track which instruction is in which block
    std::unordered_map<int, int> instruction_to_block_map;
    populate_instruction_to_block_map(cfg, blocks, instruction_to_block_map);

    // reverse the CFG and then find reverse dominance frontier (RDF)
    CFG reversed_cfg;
    reverse_cfg(cfg, reversed_cfg);
    Dominance_Analyzer post_dominance_analyzer;
    post_dominance_analyzer.perform_dominance_analysis(reversed_cfg);

    while (!worklist.empty()) {
        int i = worklist.front();
        worklist.pop();

        const auto& current_instruction = instructions[i];

        for (const auto& source : current_instruction.source) {
            if (!is_register(source)) {
                continue;
            }
            if (!definition_map.contains(source)) {
                continue;
            }

            int definition_instruction_index = definition_map[source];

            if (definition_instruction_index == PHI_DEFINITION_INDICATOR) {
                // scan through phi sources
                std::queue<std::string> phi_worklist;
                std::unordered_set<std::string> phi_visited;

                phi_worklist.push(source);
                while (!phi_worklist.empty()) {
                    std::string phi_name = phi_worklist.front();
                    phi_worklist.pop();

                    if (phi_visited.contains(phi_name)) {
                        continue;
                    }
                    phi_visited.insert(phi_name);

                    if (!phi_map.contains(phi_name)) {
                        continue;
                    }

                    const PhiFunction* phi = phi_map.at(phi_name);
                    for (const auto& [predecessor_block_id, source_name] : phi->args) {
                        if (!is_register(source_name)) {
                            continue;
                        }
                        if (!definition_map.contains(source_name)) {
                            continue;
                        }
                        int source_definition_idx = definition_map.at(source_name);
                        if (source_definition_idx == PHI_DEFINITION_INDICATOR) {
                            phi_worklist.push(source_name);
                        } else {
                            if (!marked.contains(source_definition_idx)) {
                                marked.insert(source_definition_idx);
                                worklist.push(source_definition_idx);
                            }
                        }
                    }
                }
            } else {
                if (!marked.contains(definition_instruction_index)) {
                    marked.insert(definition_instruction_index);
                    worklist.push(definition_instruction_index);
                }
            }
        }

        // special cases of use
        if (OPCODES_USING_TARGET.contains(current_instruction.opcode)) {
            // iread and fread has target stored as source
            if (!current_instruction.target.empty()) {
                auto target = current_instruction.target[0];
                if (definition_map.contains(target) && !marked.contains(definition_map[target])) {
                    marked.insert(definition_map[target]);
                    worklist.push(definition_map[target]);
                }
            }
        }

        // mark the terminating branch of each block in RDF of block containing current instruction
        int block_id = instruction_to_block_map[i];
        if (post_dominance_analyzer.dominance_info.contains(block_id)) {
            auto rdfs = post_dominance_analyzer.dominance_info.at(block_id).dominance_frontier;
            for (int rdf : rdfs) {
                // terminating branch of rdf
                int branch_idx = blocks[rdf].end_idx;
                const auto& branch_instruction = instructions[branch_idx];
                if (branch_instruction.deleted) {
                    continue;
                }
                if (!CONDITIONAL_BRANCH_OPCODES.contains(branch_instruction.opcode)) {
                    continue;
                }
                if (!marked.contains(branch_idx)) {
                    marked.insert(branch_idx);
                    worklist.push(branch_idx);
                }
            }
        }
    }

    // SWEEP PHASE

    // if the instruction is a conditional branch, replace it with an unconditional jump to the
    // nearest marked postdominator block else delete it
    for (int block_id : cfg.block_ids) {
        const auto& current_block = blocks[block_id];
        for (int i = current_block.start_idx; i <= current_block.end_idx; i++) {
            auto& current_instruction = instructions[i];

            if (current_instruction.deleted || marked.contains(i)) {
                continue;
            }

            // don't delete lables because they are branch targets
            if (!current_instruction.label.empty()) {
                continue;
            }

            if (!CONDITIONAL_BRANCH_OPCODES.contains(current_instruction.opcode)) {
                instructions[i].deleted = true;
                continue;
            }

            int nearest_marked_postdominator = find_nearest_marked_postdominator(
                blocks, instructions, post_dominance_analyzer.dominance_info, marked, block_id);

            const auto& target_block = blocks[nearest_marked_postdominator];
            const std::string target_label = instructions[target_block.start_idx].label;

            if (target_label.empty()) {
                current_instruction.deleted = true;
                continue;
            }

            current_instruction.opcode = "jumpI";
            current_instruction.source.clear();
            current_instruction.target.clear();
            current_instruction.target.push_back(target_label);
        }
    }
}

void SSA_Based_Optimizer::optimize(
    const CFG& cfg, const std::vector<BasicBlock>& blocks, std::vector<Instruction>& instructions) {

    // Dominance analysis
    Dominance_Analyzer dominance_analyzer;
    dominance_analyzer.perform_dominance_analysis(cfg);
    dominance_info_ = dominance_analyzer.dominance_info;
    dominance_tree_ = dominance_analyzer.dominance_tree;

    // Construct SSA form
    ssa_constructor_.construct_ssa_form(
        cfg, blocks, instructions, dominance_info_, dominance_tree_);

    // Optimize
    eliminate_useless_code(cfg, blocks, instructions);
    eliminate_unreachable_blocks(cfg, blocks, instructions);
}