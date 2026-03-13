#include "iloc_based_optimizer.hpp"
#include "opcode_info.hpp"

void ILOC_Based_Optimizer::compress_comparison_and_test_instructions(
    const std::vector<BasicBlock>& blocks, std::vector<Instruction>& instructions) {

    for (const auto& block : blocks) {
        for (int i = block.start_idx; i < block.end_idx; i++) {
            Instruction& instruction = instructions[i];

            if (instruction.deleted) {
                continue;
            }

            if (instruction.opcode != "comp") {
                continue;
            }

            Instruction& next_instruction = instructions[i + 1];

            if (next_instruction.deleted) {
                continue;
            }

            if (!TEST_OPCODES.contains(next_instruction.opcode)) {
                continue;
            }

            std::string new_opcode;
            if (next_instruction.opcode == "testeq") {
                new_opcode = "cmp_EQ";
            } else if (next_instruction.opcode == "testne") {
                new_opcode = "cmp_NE";
            } else if (next_instruction.opcode == "testlt") {
                new_opcode = "cmp_LT";
            } else if (next_instruction.opcode == "testle") {
                new_opcode = "cmp_LE";
            } else if (next_instruction.opcode == "testgt") {
                new_opcode = "cmp_GT";
            } else if (next_instruction.opcode == "testge") {
                new_opcode = "cmp_GE";
            } else {
                continue;
            }

            instruction.opcode = new_opcode;
            instruction.target.clear();
            instruction.target.push_back(next_instruction.target[0]);
            next_instruction.deleted = true;
        }
    }
}