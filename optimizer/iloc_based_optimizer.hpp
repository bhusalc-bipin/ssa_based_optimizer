#pragma once

#include "cfg/basic_block.hpp"

#include <vector>

class ILOC_Based_Optimizer {
private:
    // combine comparison and test instructions into single comparison instruction. Example: comp
    // followed by testle to become a single cmp_LE instruction.
    void compress_comparison_and_test_instructions(
        const std::vector<BasicBlock>& blocks, std::vector<Instruction>& instructions);

public:
    void optimize(const std::vector<BasicBlock>& blocks, std::vector<Instruction>& instructions) {
        compress_comparison_and_test_instructions(blocks, instructions);
    }
};