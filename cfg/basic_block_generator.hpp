#pragma once

#include <string>
#include <vector>

#include "cfg/basic_block.hpp"

class BasicBlockGenerator {
public:
    // NOTE: Don't delete or manually add any instructions from/to the instructions_ vector. You can
    // modify the contents of instructions itself but don't delete or manually add new or move
    // instructions here and there because blocks_ uses the index of the instruction to determine
    // the start and end of the basic block.
    std::vector<Instruction> instructions_;
    std::vector<BasicBlock> blocks_;

    // Parse an iLOC file then populate the instructions_ vector
    void parse_iloc_file(const std::string& filepath);

    // Build basic blocks from the instructions_ vector then populate the blocks_ vector
    void build_basic_blocks();
};