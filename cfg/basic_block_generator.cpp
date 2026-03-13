#include "cfg/basic_block_generator.hpp"
#include "opcode_info.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

// Helper function to parse a comma-separated string into a vector of strings
void parse_comma_separated_string(std::string str, std::vector<std::string>& output) {
    std::istringstream iss(str);
    std::string token;

    while (std::getline(iss, token, ',')) {
        // trim leading/trailing whitespace from each token
        auto token_start = token.find_first_not_of(" \t");
        auto token_end = token.find_last_not_of(" \t");

        if (token_start != std::string::npos) {
            token = token.substr(token_start, token_end - token_start + 1);
            output.push_back(token);
        }
    }
}

void BasicBlockGenerator::parse_iloc_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open the file: " + filepath);
    }

    std::string line;
    int instruction_id = 0;

    while (std::getline(file, line)) {
        if (line.empty() || line.find_first_not_of(" \t\r\n") == std::string::npos) {
            continue;
        }

        Instruction instruction;
        instruction.id = instruction_id++;

        // remove leading whitespace
        line = line.substr(line.find_first_not_of(" \t"));

        // Parse LABELS (only check the first token for a colon, not the whole line, this prevents
        // false matches on colons inside the quoted strings. Example: .string "A:")
        // Store label in instruction.label and "nop" as opcode
        std::string first_token = line.substr(0, line.find_first_of(" \t"));
        if (auto colon_pos = first_token.find(':'); colon_pos != std::string::npos) {
            instruction.label = line.substr(0, colon_pos);

            // Check if there is an instruction after the label on the same line
            std::string str_after_colon = line.substr(colon_pos + 1);
            auto first_non_space_after_colon = str_after_colon.find_first_not_of(" \t");

            if (first_non_space_after_colon == std::string::npos) {
                // No instruction after label
                instruction.opcode = "nop";
                instructions_.push_back(instruction);
                continue;
            } else {
                // There is an instruction after the label, continue parsing that
                line = str_after_colon.substr(first_non_space_after_colon);
            }
        }

        // Parse other instructions (or instructions after labels)
        std::istringstream iss(line);
        std::string token;
        iss >> token; // First token (opcode or pseudo-op)

        instruction.opcode = token;

        // Check if it's a PSEUDO_OP
        if (PSEUDO_OPS.find(token) != PSEUDO_OPS.end()) {
            instruction.is_pseudo_ops = true;
        }

        // Parse rest of the string for source and target operands but first check if there are any
        if (!std::getline(iss, token)) {
            // No operands
            instructions_.push_back(instruction);
            continue;
        }

        // Source and target operands are separated by "=>" or "->" (if target operands exist)
        if (auto arrow_end_pos = token.find(">"); arrow_end_pos != std::string::npos) {
            std::string source_operand_str = token.substr(0, arrow_end_pos - 1);
            std::string target_operand_str = token.substr(arrow_end_pos + 1);

            // Parse source and target operand strings into vectors delimited by ','
            parse_comma_separated_string(source_operand_str, instruction.source);
            parse_comma_separated_string(target_operand_str, instruction.target);
        } else {
            // No "=>" or "->", everything is sources (e.g., "ret", "nop")
            parse_comma_separated_string(token, instruction.source);
        }

        instructions_.push_back(instruction);
    }
    file.close();
}

void BasicBlockGenerator::build_basic_blocks() {
    if (instructions_.empty()) {
        return;
    }

    int block_id = 0;
    BasicBlock current_block;
    bool block_started = false; // flag to indicate if we are in the middle of a block
    std::string current_procedure_name;

    for (size_t i = 0; i < instructions_.size(); i++) {
        const auto& current_instruction = instructions_[i];

        // Skip pseudo-ops, they are not real instructions that we intend to operate on
        if (current_instruction.is_pseudo_ops) {
            // Track procedure name from .frame
            if (current_instruction.opcode == ".frame") {
                current_procedure_name = current_instruction.source[0];
            }

            // If current instruction is pseduo_op and we are in the middle of a block then end the
            // block before the pseudo-op (This handles the beginning of a function too as it is
            // marked by .frame pseudo-op)
            if (block_started) {
                current_block.end_idx = i - 1;
                blocks_.push_back(current_block);
                block_started = false;
            }
            continue; // ignore pseudo-ops
        }

        // Start a new block if needed (after pseudo-ops or after the previous block ended)
        if (!block_started) {
            current_block.id = block_id;
            current_block.start_idx = i;
            current_block.procedure_name = current_procedure_name;
            block_started = true;
        }

        // Check if this instruction ends the current basic block
        bool is_block_ending_instruction = false;
        bool is_last_instruction = (i == instructions_.size() - 1);

        // Case 1: Last instruction always ends a block
        if (is_last_instruction) {
            is_block_ending_instruction = true;
        }

        // Case 2: If the next instruction is a label then current block ends here
        if (!is_last_instruction && !instructions_[i + 1].label.empty()) {
            is_block_ending_instruction = true;
        }

        // Case 3: Branch instructions end the block
        if (BRANCH_OPS.find(current_instruction.opcode) != BRANCH_OPS.end()) {
            is_block_ending_instruction = true;
        }

        // Case 4: Begining of a new procedure always ends the old block and starts a new (procedure
        // start detected by .frame pseudo-op).
        // Already handled above by skipping pseudo-ops and ending blocks before them

        if (is_block_ending_instruction && block_started) {
            current_block.end_idx = i;
            blocks_.push_back(current_block);
            block_id++;
            block_started = false;
        }
    }
}