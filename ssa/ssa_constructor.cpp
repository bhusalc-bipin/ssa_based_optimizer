#include "ssa/ssa_constructor.hpp"
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

std::string SSA_Constructor::make_expr_key(const Instruction& current_instruction) {
    if (current_instruction.target.empty()
        || OPCODES_USING_TARGET.contains(current_instruction.opcode)
        || OPCODES_WITH_SIDE_EFFECT.contains(current_instruction.opcode)
        || BRANCH_OPS.contains(current_instruction.opcode) || current_instruction.opcode == "nop") {
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
        }
    }
}