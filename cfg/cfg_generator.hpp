#pragma once

#include "basic_block.hpp"

#include <string>
#include <unordered_map>
#include <vector>

static constexpr int ENTRY_BLOCK_ID = -1;
static constexpr int EXIT_BLOCK_ID = -2;

struct CFG {
    std::string procedure_name;

    // list of block ids that belong to this procedure
    std::vector<int> block_ids;

    // mapping from block id to its predecessor block ids
    std::unordered_map<int, std::vector<int>> predecessors;

    // mapping from block id to its successor block ids
    std::unordered_map<int, std::vector<int>> successors;
};

class CFG_Generator {
public:
    std::vector<CFG> cfgs_; // list of control flow graphs

    void build_cfg(std::vector<BasicBlock>& blocks, std::vector<Instruction>& instructions);
};