#pragma once

#include "cfg/basic_block.hpp"

#include <vector>

class SSA_Deconstructor {
public:
    void deconstruct_ssa(std::vector<Instruction>& instructions);
};