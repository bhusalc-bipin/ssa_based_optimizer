#pragma once

#include "cfg/basic_block.hpp"
#include "cfg/cfg_generator.hpp"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct LivenessInfo {
    std::unordered_set<std::string> live_in;
    std::unordered_set<std::string> live_out;
};

class Liveness_Analyzer {
private:
    // map from block id to upward exposed variables
    std::unordered_map<int, std::unordered_set<std::string>> ue_vars;
    // map from block id to variables killed in the block
    std::unordered_map<int, std::unordered_set<std::string>> var_kill;

    void compute_local_information(const CFG& cfg, const std::vector<BasicBlock>& blocks,
        const std::vector<Instruction>& instructions);

    void solve_dataflow_equations(const CFG& cfg);

public:
    // map from block id to its liveness info
    std::unordered_map<int, LivenessInfo> liveness_info;

    void perform_liveness_analysis(const CFG& cfg, const std::vector<BasicBlock>& blocks,
        const std::vector<Instruction>& instructions);
};