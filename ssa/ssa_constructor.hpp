#pragma once

#include "analysis/dominance_analyzer.hpp"
#include "analysis/liveness_analyzer.hpp"
#include "cfg/basic_block.hpp"
#include "cfg/cfg_generator.hpp"

#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct PhiFunction {
    std::string base_name; // original variable name before renaming
    std::string target; // variable defined by the phi function (renamed in optrename)
    // mapping from predecessor block id to source variable name
    std::unordered_map<int, std::string> args;
};

/*
 * SSA Constructor class for building SSA form with integrated DBRE
 */
class SSA_Constructor {
private:
    // counter for base_name, used to generate unique SSA names
    std::unordered_map<std::string, int> name_counter_;

    // rename stack for base_name, where top = current SSA name in scope
    std::unordered_map<std::string, std::stack<std::string>> name_stacks_;

    // scoped AVAIL table, where each entry is one block's scope (push on StartBlock, pop on
    // EndBlock), and key = expression key, value = SSA name that first computed it
    std::vector<std::unordered_map<std::string, std::string>> avail_;

    // redundant instructions
    std::unordered_set<int> dead_;

    // generate a new SSA name for given base register (e.g. "%vr1" -> "%vr1_2")
    std::string new_name(const std::string& base);

    // return current SSA name for base (top of stack) or base itself if stack empty
    std::string top_name(const std::string& base);

    // look up expression key in AVAIL (search all visible scopes from inner to outer level)
    // Return the SSA name that computed it or if not found then return empty string
    std::string lookup_avail(const std::string& key);

    // insert expression key into the current block's AVAIL scope
    void insert_avail(const std::string& key, const std::string& ssa_name);

    // Build and return expression key for instruction after sources are renamed
    // Return empty string for instructions not eligible (instruction wit side-effect or the ones
    // that don't have target or branch instruction) for AVAIL
    std::string make_expr_key(const Instruction& instr);

    // compute the iterated dominance frontier
    std::unordered_set<int> compute_idf(const std::unordered_set<int>& S,
        const std::unordered_map<int, DominanceInfo>& dominance_info);

    // find globals (variables that are live in multiple blocks) and block sets (which variable is
    // defined in which blocks)
    void find_globals_and_block_sets(const CFG& cfg, const std::vector<BasicBlock>& blocks,
        const std::vector<Instruction>& instructions, std::unordered_set<std::string>& globals,
        std::unordered_map<std::string, std::unordered_set<int>>& block_sets);

    void insert_phi_functions(const CFG& cfg, const std::vector<BasicBlock>& blocks,
        const std::vector<Instruction>& instructions,
        const std::unordered_map<int, DominanceInfo>& dominance_info);

    // performe rename and DBRE optimization
    void optrename(int block_id, const CFG& cfg, const std::vector<BasicBlock>& blocks,
        std::vector<Instruction>& instructions,
        const std::unordered_map<int, DominanceInfo>& dominance_info,
        const std::unordered_map<int, std::vector<int>>& dominance_tree);

public:
    // mapping from block id to list of phi functions for that block
    std::unordered_map<int, std::vector<PhiFunction>> phi_functions;

    // construct pruned SSA form with integrated DBRE for the given CFG
    void construct_ssa_form(const CFG& cfg, const std::vector<BasicBlock>& blocks,
        std::vector<Instruction>& instructions,
        const std::unordered_map<int, DominanceInfo>& dominance_info,
        const std::unordered_map<int, std::vector<int>>& dominance_tree);
};