#include "ssa/ssa_constructor.hpp"
#include "analysis/liveness_analyzer.hpp"
#include "cfg/cfg_generator.hpp"
#include "opcode_info.hpp"

#include <queue>
#include <unordered_set>

static bool is_register(const std::string& name) {
    return name.starts_with("%vr");
}

std::string SSA_Constructor::new_name(const std::string& base) {
    int index = name_counter_[base];
    name_counter_[base]++;
    return base + "_" + std::to_string(index);
}

std::string SSA_Constructor::top_name(const std::string& base) {
    auto it = name_stacks_.find(base);
    if (it == name_stacks_.end() || it->second.empty()) {
        return base; // no ssa name, so return original base name
    }
    return it->second.top();
}

std::string SSA_Constructor::lookup_avail(const std::string& key) {
    // from inner to outer scope look for key in avail
    for (std::size_t i = avail_.size(); i > 0; i--) {
        auto found = avail_[i - 1].find(key);
        if (found != avail_[i - 1].end()) {
            return found->second;
        }
    }
    return "";
}

void SSA_Constructor::insert_avail(const std::string& key, const std::string& ssa_name) {
    avail_.back()[key] = ssa_name;
}

std::string SSA_Constructor::generate_expression_key(const Instruction& current_instruction) {
    if (current_instruction.target.empty()
        || OPCODES_USING_TARGET.contains(current_instruction.opcode)
        || OPCODES_WITH_SIDE_EFFECT.contains(current_instruction.opcode)
        || BRANCH_OPS.contains(current_instruction.opcode) || current_instruction.opcode == "nop") {
        return "";
    }

    if (MOVE_OPCODES.contains(current_instruction.opcode)) {
        return "";
    }

    if (current_instruction.source.empty()) {
        return current_instruction.opcode;
    }

    if (current_instruction.source.size() == 1) {
        return current_instruction.opcode + "," + current_instruction.source[0];
    }

    std::string operand1 = current_instruction.source[0];
    std::string operand2 = current_instruction.source[1];

    if (COMMUTATIVE_OPCODES.contains(current_instruction.opcode) && operand1 > operand2) {
        std::swap(operand1, operand2);
    }

    return current_instruction.opcode + "," + operand1 + "," + operand2;
}

std::unordered_set<int> SSA_Constructor::compute_idf(const std::unordered_set<int>& S,
    const std::unordered_map<int, DominanceInfo>& dominance_info) {

    std::unordered_set<int> idf;
    std::queue<int> worklist;

    for (int block : S) {
        worklist.push(block);
    }

    while (!worklist.empty()) {
        int current_block = worklist.front();
        worklist.pop();
        if (!dominance_info.contains(current_block)) {
            continue; // unreachable block in CFG
        }
        for (int c : dominance_info.at(current_block).dominance_frontier) {
            if (!idf.contains(c)) {
                idf.insert(c);
                worklist.push(c);
            }
        }
    }
    return idf;
}

void SSA_Constructor::find_globals_and_block_sets(const CFG& cfg,
    const std::vector<BasicBlock>& blocks, const std::vector<Instruction>& instructions,
    std::unordered_set<std::string>& globals,
    std::unordered_map<std::string, std::unordered_set<int>>& block_sets) {

    for (const auto& block_id : cfg.block_ids) {
        const auto& current_block = blocks[block_id];
        std::unordered_set<std::string> var_kill;

        for (int i = current_block.start_idx; i <= current_block.end_idx; i++) {
            const auto& current_instruction = instructions[i];

            // process sources (uses): if not already killed in thie block, it's global
            // NOTE: iread/fread source (actual target but parsed as source) is also a use
            for (const auto& src : current_instruction.source) {
                if (is_register(src) && !var_kill.contains(src)) {
                    globals.insert(src);
                }
            }

            if (current_instruction.target.empty()) {
                continue;
            }

            auto target = current_instruction.target[0];

            // process opcodes that use their target as source (store, storeAI etc.). These opcode's
            // target operands are addresses (uses), not definitions. Special case (due to parser
            // implementation): iread/fread is handled above
            if (OPCODES_USING_TARGET.contains(current_instruction.opcode)) {
                if (is_register(target) && !var_kill.contains(target)) {
                    globals.insert(target);
                }
                continue; // no definitons for any of these opcodes
            }

            // process definitions (target)
            if (is_register(target)) {
                var_kill.insert(target);
                block_sets[target].insert(block_id);
            }
        }
    }
}

void SSA_Constructor::insert_phi_functions(const CFG& cfg, const std::vector<BasicBlock>& blocks,
    const std::vector<Instruction>& instructions,
    const std::unordered_map<int, DominanceInfo>& dominance_info) {

    Liveness_Analyzer liveness_analyzer;
    liveness_analyzer.perform_liveness_analysis(cfg, blocks, instructions);

    std::unordered_set<std::string> globals;
    std::unordered_map<std::string, std::unordered_set<int>> block_sets;
    find_globals_and_block_sets(cfg, blocks, instructions, globals, block_sets);

    const int entry_block = cfg.block_ids[0];

    for (const auto& var : globals) {
        std::unordered_set<int> S;
        if (block_sets.count(var)) {
            S = block_sets.at(var);
        }
        S.insert(entry_block);

        for (const auto& block : compute_idf(S, dominance_info)) {
            if (!liveness_analyzer.liveness_info.contains(block)) {
                continue; // unreachable block
            }
            if (!liveness_analyzer.liveness_info.at(block).live_in.contains(var)) {
                continue;
            }

            // check if phi function for var already exists in this block and if yes then no need to
            // insert again
            bool phi_alredy_exists = false;
            if (phi_functions.contains(block)) {
                for (const auto& phi : phi_functions.at(block)) {
                    if (phi.base_name == var) {
                        phi_alredy_exists = true;
                        break;
                    }
                }
            }

            if (phi_alredy_exists) {
                continue;
            }

            // if phi function doesn't already exist for this variable in this block, insert new phi
            // function
            PhiFunction new_phi;
            new_phi.base_name = var;
            // using var as placeholder for target for now, it will be renamed later in rename phase
            new_phi.target = var;
            if (cfg.predecessors.contains(block)) {
                for (int predecessor : cfg.predecessors.at(block)) {
                    if (predecessor == ENTRY_BLOCK_ID) {
                        continue; // skip the dummy entry block
                    }
                    // using var as placeholder for source for now, it will be renamed later in
                    // rename phase
                    new_phi.args[predecessor] = var;
                }
            }
            phi_functions[block].push_back(new_phi);
        }
    }
}

void SSA_Constructor::optrename(int block_id, const CFG& cfg, const std::vector<BasicBlock>& blocks,
    std::vector<Instruction>& instructions,
    const std::unordered_map<int, DominanceInfo>& dominance_info,
    const std::unordered_map<int, std::vector<int>>& dominance_tree) {

    // for each phi function in this block, push newname to name stack
    if (phi_functions.contains(block_id)) {
        for (auto& phi : phi_functions[block_id]) {
            name_stacks_[phi.base_name].push(new_name(phi.base_name));
        }
    }

    // StartBlock
    avail_.push_back({});

    // process all instructions in this block in execution order then rename registers as needed and
    // track dead instructions
    const auto& current_block = blocks[block_id];
    for (int i = current_block.start_idx; i <= current_block.end_idx; i++) {
        auto& current_instruction = instructions[i];

        // First process uses (sources) then definitions (targets) for each instruction

        // rename sources
        for (auto& src : current_instruction.source) {
            if (is_register(src)) {
                src = top_name(src);
            }
        }

        // special case for uses
        // process the opcodes where target is the use, rename it
        if (OPCODES_USING_TARGET.contains(current_instruction.opcode)) {
            for (auto& tgt : current_instruction.target) {
                if (is_register(tgt))
                    tgt = top_name(tgt);
            }
            // no definitions for any of these opcodes, so skip rest of loop and move to next
            // instruction
            continue;
        }

        // skip the instructions that don't have target or target is not a register (example jumpI)
        if (current_instruction.target.empty() || !is_register(current_instruction.target[0])) {
            continue;
        }

        // if the expression is already computed and available then reuse the previously computed
        // value instead of recomputing it
        std::string expression_key = generate_expression_key(current_instruction);
        const std::string current_base_name = current_instruction.target[0];

        if (!expression_key.empty()) {
            std::string available_name = lookup_avail(expression_key);
            if (!available_name.empty()) {
                // redundant instruction, so reuse exisiting ssa name and track current instruction
                // as dead
                name_stacks_[current_base_name].push(available_name);
                dead_.insert(i);
            } else {
                std::string new_ssa_name = new_name(current_base_name);
                name_stacks_[current_base_name].push(new_ssa_name);
                insert_avail(expression_key, new_ssa_name);
            }
        } else {
            // this instruction is not eligible for avail, so just assign a new ssa name for its
            // target
            name_stacks_[current_base_name].push(new_name(current_base_name));
        }
    }

    // update phi function arguments of the current block's successors with the current ssa name of
    // the variable defined in this block
    if (cfg.successors.contains(block_id)) {
        for (int successor : cfg.successors.at(block_id)) {
            if (successor == EXIT_BLOCK_ID) {
                continue; // skip the dummy exit block
            }
            if (!phi_functions.contains(successor)) {
                continue;
            }
            for (auto& phi : phi_functions[successor]) {
                if (phi.args.contains(block_id)) {
                    phi.args[block_id] = top_name(phi.base_name);
                }
            }
        }
    }

    // recursively process current block's children in dominator tree
    if (dominance_tree.contains(block_id)) {
        for (int child : dominance_tree.at(block_id)) {
            optrename(child, cfg, blocks, instructions, dominance_info, dominance_tree);
        }
    }

    // assign final ssa names and mark dead instructions as deleted in reverse execution order
    for (int i = current_block.end_idx; i >= current_block.start_idx; i--) {
        auto& current_instruction = instructions[i];

        if (OPCODES_USING_TARGET.contains(current_instruction.opcode)
            || current_instruction.target.empty() || !is_register(current_instruction.target[0])) {
            continue;
        }

        std::string current_base_name = current_instruction.target[0];
        if (!name_stacks_.contains(current_base_name) || name_stacks_[current_base_name].empty()) {
            continue;
        }

        std::string ssa_name = name_stacks_[current_base_name].top();
        name_stacks_[current_base_name].pop();

        if (dead_.contains(i)) {
            current_instruction.deleted = true;
        } else {
            current_instruction.target[0] = ssa_name;
        }
    }

    // assign final ssa names to phi targets
    if (phi_functions.contains(block_id)) {
        for (auto& phi : phi_functions[block_id]) {
            if (name_stacks_.contains(phi.base_name) && !name_stacks_[phi.base_name].empty()) {
                phi.target = name_stacks_[phi.base_name].top();
                name_stacks_[phi.base_name].pop();
            }
        }
    }

    // EndBlock
    avail_.pop_back();
}

void SSA_Constructor::construct_ssa_form(const CFG& cfg, const std::vector<BasicBlock>& blocks,
    std::vector<Instruction>& instructions,
    const std::unordered_map<int, DominanceInfo>& dominance_info,
    const std::unordered_map<int, std::vector<int>>& dominance_tree) {

    name_counter_.clear();
    name_stacks_.clear();
    avail_.clear();
    dead_.clear();
    phi_functions.clear();

    insert_phi_functions(cfg, blocks, instructions, dominance_info);
    optrename(cfg.block_ids[0], cfg, blocks, instructions, dominance_info, dominance_tree);
}