#include "ssa/ssa_constructor.hpp"
#include "opcode_info.hpp"

void SSA_Constructor::find_global_names(const CFG& cfg, const std::vector<BasicBlock>& blocks,
    const std::vector<Instruction>& instructions) {

    globals_.clear();
    block_sets_.clear();

    for (const auto& block_id : cfg.block_ids) {
        const auto& current_block = blocks[block_id];
        std::unordered_set<std::string> var_kill;

        for (int i = current_block.start_idx; i <= current_block.end_idx; i++) {
            const auto& current_instruction = instructions[i];

            // process sources (uses): if not already killed in this block, it's global
            // Note: iread/fread source (actual target but parsed as source) is also a use
            for (const auto& src : current_instruction.source) {
                if (src.starts_with("%vr") && !var_kill.contains(src)) {
                    globals_.insert(src);
                }
            }

            // opcodes that use their target as source (store, storeAI, storeAO, etc.). These
            // opcode's target operands are addresses (uses), not definitions. Special case (due to
            // parser implementation): iread/fread is handled above
            if (OPCODES_USING_TARGET.contains(current_instruction.opcode)) {
                for (const auto& tgt : current_instruction.target) {
                    if (tgt.starts_with("%vr") && !var_kill.contains(tgt)) {
                        globals_.insert(tgt);
                    }
                }
                continue; // no definitions for any of these opcodes
            }

            // Process definitions (targets)
            for (const auto& tgt : current_instruction.target) {
                if (tgt.starts_with("%vr")) {
                    var_kill.insert(tgt);
                    block_sets_[tgt].insert(block_id);
                }
            }
        }
    }
}

void SSA_Constructor::insert_phi_functions(
    const std::unordered_map<int, DominanceInfo>& dominance_info) {

    phi_functions.clear();

    for (const auto& current_variable : globals_) {
        if (!block_sets_.contains(current_variable)) {
            continue; // used but never defined
        }

        std::unordered_set<int> work_list = block_sets_[current_variable];
        // Track where phi functions have already been inserted for this variable
        std::unordered_set<int> has_phi;

        while (!work_list.empty()) {
            // Remove the block from WorkList
            auto it = work_list.begin();
            int b = *it;
            work_list.erase(it);

            for (const auto& d : dominance_info.at(b).dominance_frontier) {
                if (!has_phi.contains(d)) {
                    // Insert a phi function for current variable in this block and add block to
                    // work list
                    PhiFunction phi;
                    phi.base_name = current_variable;
                    phi.target = current_variable; // will be renamed later in the rename step
                    phi_functions[d].push_back(phi);
                    has_phi.insert(d);
                    work_list.insert(d);
                }
            }
        }
    }
}

std::string SSA_Constructor::new_name(const std::string& base_name) {
    int i = counter_[base_name];
    counter_[base_name] = i + 1;
    stack_[base_name].push(i);
    return base_name + "_" + std::to_string(i);
}

std::string SSA_Constructor::current_name(const std::string& base_name) {
    if (!stack_.contains(base_name) || stack_[base_name].empty()) {
        // Give subscript 0 to variable used before any definition in this path.
        // This can happen for function parameters.
        return new_name(base_name);
    }
    return base_name + "_" + std::to_string(stack_[base_name].top());
}

void SSA_Constructor::rename(int block_id, const CFG& cfg, const std::vector<BasicBlock>& blocks,
    std::vector<Instruction>& instructions,
    const std::unordered_map<int, DominanceInfo>& dominance_info,
    const std::unordered_map<int, std::vector<int>>& dom_tree) {

    // Track how many names we push in this block so we can pop them when we leave
    // map base variable name to number of pushes in this block
    std::unordered_map<std::string, int> push_count;

    // rename the targets of each phi function in this block
    if (phi_functions.contains(block_id)) {
        for (auto& phi : phi_functions[block_id]) {
            std::string base = phi.target;
            phi.target = new_name(base);
            push_count[base]++;
        }
    }

    // rename uses and defs in each non-phi instruction in this block
    const auto& current_block = blocks[block_id];
    for (int i = current_block.start_idx; i <= current_block.end_idx; i++) {
        auto& current_instruction = instructions[i];

        // OPCODES_USING_TARGET (store, storeAI, storeAO, iread, fread, etc.) all operands are uses,
        // no definitions
        if (OPCODES_USING_TARGET.contains(current_instruction.opcode)) {
            for (auto& src : current_instruction.source) {
                if (src.starts_with("%vr") && globals_.contains(src)) {
                    src = current_name(src);
                }
            }
            for (auto& tgt : current_instruction.target) {
                if (tgt.starts_with("%vr") && globals_.contains(tgt)) {
                    tgt = current_name(tgt);
                }
            }
            continue;
        }

        // Normal instructions: rename sources as uses first, then targets as defs
        for (auto& src : current_instruction.source) {
            if (src.starts_with("%vr") && globals_.contains(src)) {
                src = current_name(src);
            }
        }
        for (auto& tgt : current_instruction.target) {
            if (tgt.starts_with("%vr") && globals_.contains(tgt)) {
                std::string base = tgt;
                tgt = new_name(base);
                push_count[base]++;
            }
        }
    }

    // Fill in phi-function parameters in each CFG successor of this block
    if (cfg.successors.contains(block_id)) {
        for (int current_successor : cfg.successors.at(block_id)) {
            if (current_successor == EXIT_BLOCK_ID || !phi_functions.contains(current_successor)) {
                continue;
            }
            for (auto& phi : phi_functions[current_successor]) {
                phi.args[block_id] = current_name(phi.base_name);
            }
        }
    }

    // Recurse into dominator tree children
    if (dom_tree.contains(block_id)) {
        for (int child : dom_tree.at(block_id)) {
            rename(child, cfg, blocks, instructions, dominance_info, dom_tree);
        }
    }

    // Pop all names pushed in this block
    for (const auto& [base, count] : push_count) {
        for (int j = 0; j < count; j++) {
            stack_[base].pop();
        }
    }
}

void SSA_Constructor::rename_variables(const CFG& cfg, const std::vector<BasicBlock>& blocks,
    std::vector<Instruction>& instructions,
    const std::unordered_map<int, DominanceInfo>& dominance_info) {

    counter_.clear();
    stack_.clear();
    for (const auto& name : globals_) {
        counter_[name] = 0;
        // no need to initialize the stack explicitly because it starts empty by default
    }

    // Build dominator tree: parent -> list of children
    // Dominance info gives us immediate dominators (child -> parent), but rename algorithm needs to
    // traverse from root to leaves, so we build the tree structure first. So, to represent the
    // dominator tree, we can invert the mapping from block to its immediate dominator into a
    // mapping from block to list of blocks it immediately dominates.
    std::unordered_map<int, std::vector<int>> dom_tree;
    for (int block_id : cfg.block_ids) {
        if (!dominance_info.contains(block_id)) {
            continue;
        }
        int idom = dominance_info.at(block_id).immediate_dominator;
        if (idom != -1) {
            dom_tree[idom].push_back(block_id);
        }
    }

    // Start renaming from the root of the CFG
    rename(cfg.block_ids[0], cfg, blocks, instructions, dominance_info, dom_tree);
}

void SSA_Constructor::construct_ssa_form(const CFG& cfg, const std::vector<BasicBlock>& blocks,
    std::vector<Instruction>& instructions,
    const std::unordered_map<int, DominanceInfo>& dominance_info) {

    find_global_names(cfg, blocks, instructions);
    insert_phi_functions(dominance_info);
    rename_variables(cfg, blocks, instructions, dominance_info);
}