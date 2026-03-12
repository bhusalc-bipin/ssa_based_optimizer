#pragma once

#include "analysis/dominance_analyzer.hpp"
#include "cfg/basic_block.hpp"
#include "cfg/cfg_generator.hpp"

#include <string>
#include <unordered_map>
#include <vector>

struct PhiFunction {
    std::string base_name; // original variable name before renaming
    std::string target; // variable defined by the phi function (will be renamed in the rename step)
    // mapping from predecessor block id to source variable name
    std::unordered_map<int, std::string> args;
};

class SSA_Constructor {
private:
    // Insert phi functions
    void insert_phi_functions(const std::unordered_map<int, DominanceInfo>& dominance_info);

    // Rename registers in the instructions and phi functions
    void rename(const CFG& cfg, const std::vector<BasicBlock>& blocks,
        std::vector<Instruction>& instructions,
        const std::unordered_map<int, DominanceInfo>& dominance_info);

public:
    // mapping from block id to list of all phi functions for this block
    std::unordered_map<int, std::vector<PhiFunction>> phi_functions;

    // construct pruned-SSA form for the given CFG
    void construct_ssa_form(const CFG& cfg, const std::vector<BasicBlock>& blocks,
        std::vector<Instruction>& instructions,
        const std::unordered_map<int, DominanceInfo>& dominance_info);
};