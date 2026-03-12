#pragma once

#include "analysis/dominance_analyzer.hpp"
#include "cfg/basic_block.hpp"
#include "cfg/cfg_generator.hpp"

#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct PhiFunction {
    std::string base_name; // original variable name before renaming
    std::string target; // variable defined by the phi function (will be renamed in the rename step)
    // mapping from predecessor block id to source variable name
    std::unordered_map<int, std::string> args;
};

class SSA_Constructor {
private:
    // variables that are live in multiple blocks (i.e., variables that need phi nodes)
    std::unordered_set<std::string> globals_;
    // mapping from variable names to definitions (blocks where the variable is defined)
    std::unordered_map<std::string, std::unordered_set<int>> block_sets_;

    // counter for each variable
    std::unordered_map<std::string, int> counter_;
    // stack for each variable
    std::unordered_map<std::string, std::stack<int>> stack_;

    // Find globals and block sets for all variables in the CFG.
    void find_global_names(const CFG& cfg, const std::vector<BasicBlock>& blocks,
        const std::vector<Instruction>& instructions);

    // Insert phi functions
    void insert_phi_functions(const std::unordered_map<int, DominanceInfo>& dominance_info);

    // Generate a new SSA name: base_name + "_" + counter, then push subscript onto stack
    std::string new_name(const std::string& base_name);

    // Get current SSA name for global variable (top of stack)
    std::string current_name(const std::string& base_name);

    // Recursive rename walk over the dominator tree
    void rename(int block_id, const CFG& cfg, const std::vector<BasicBlock>& blocks,
        std::vector<Instruction>& instructions,
        const std::unordered_map<int, DominanceInfo>& dominance_info,
        const std::unordered_map<int, std::vector<int>>& dom_tree);

    // Top level rename function to rename the variables in the CFG
    void rename_variables(const CFG& cfg, const std::vector<BasicBlock>& blocks,
        std::vector<Instruction>& instructions,
        const std::unordered_map<int, DominanceInfo>& dominance_info);

public:
    // mapping from block id to list of all phi functions for this block
    std::unordered_map<int, std::vector<PhiFunction>> phi_functions;

    // construct semipruned SSA form for the given CFG and dominance information
    void construct_ssa_form(const CFG& cfg, const std::vector<BasicBlock>& blocks,
        std::vector<Instruction>& instructions,
        const std::unordered_map<int, DominanceInfo>& dominance_info);
};