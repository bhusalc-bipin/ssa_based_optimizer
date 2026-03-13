#include "cfg/basic_block_generator.hpp"
#include "cfg/cfg_generator.hpp"
#include "opcode_info.hpp"
#include "optimizer/ssa_based_optimizer.hpp"
#include "ssa/ssa_deconstructor.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

void write_iloc_file(
    const std::vector<Instruction>& instructions, const std::string& output_filepath) {

    std::ofstream outfile(output_filepath);
    if (!outfile.is_open()) {
        throw std::runtime_error("Failed to open the output file: " + output_filepath);
    }

    for (const auto& instruction : instructions) {
        // Skip deleted instructions
        if (instruction.deleted) {
            continue;
        }

        // Add indentation for non-labels
        if (instruction.label.empty()) {
            outfile << "\t";
        }

        // Print label if exists
        if (!instruction.label.empty()) {
            outfile << instruction.label << ":\t";
        }

        // Print opcode
        outfile << instruction.opcode;

        // Print source
        if (!instruction.source.empty()) {
            outfile << "\t";
            for (size_t i = 0; i < instruction.source.size(); i++) {
                outfile << instruction.source[i];
                if (i < instruction.source.size() - 1) {
                    outfile << ", ";
                }
            }
        }

        // Print target
        if (!instruction.target.empty()) {
            // Use "->" for branch ops with target (except icall and fcall). For other use "=>"
            if (BRANCH_OPS.contains(instruction.opcode) && instruction.opcode != "icall"
                && instruction.opcode != "fcall") {
                outfile << " -> ";
            } else {
                outfile << " => ";
            }
            for (size_t i = 0; i < instruction.target.size(); i++) {
                outfile << instruction.target[i];
                if (i < instruction.target.size() - 1) {
                    outfile << ", ";
                }
            }
        }
        outfile << "\n";
    }
    outfile.close();
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <input_iloc_file>\n";
        return 1;
    }

    std::string input_filepath = argv[1];

    // Extract filename without path and extension
    std::filesystem::path input_path(input_filepath);
    // "fib.il" from "input/fib.il" or "fib.lvn.il" from "input/fib.lvn.il"
    std::string filename = input_path.filename().string();
    auto dot_position = filename.find(".");
    if (dot_position != std::string::npos) {
        // "fib" from "fib.il" or "fib.lvn.il"
        filename = filename.substr(0, dot_position);
    }

    std::string output_dir = "output";

    // Create output directory if it doesn't exist
    std::filesystem::create_directories(output_dir);

    // Create output filepath
    std::string output_filepath = output_dir + "/" + filename + ".dbre.il";

    // Step 1: Parse the input iloc file
    BasicBlockGenerator bbg;
    bbg.parse_iloc_file(input_filepath);

    // Step 2: Build the basic blocks
    bbg.build_basic_blocks();

    // Step 3: Build CFG for each procedure
    CFG_Generator config_generator;
    config_generator.build_cfg(bbg.blocks_, bbg.instructions_);

    // Step 4: Run SSA-based optimization for each procedure
    SSA_Based_Optimizer ssa_based_optimizer;
    SSA_Deconstructor ssa_deconstructor;
    for (const auto& cfg : config_generator.cfgs_) {
        ssa_based_optimizer.optimize(cfg, bbg.blocks_, bbg.instructions_);
        ssa_deconstructor.deconstruct_ssa(bbg.instructions_);
    }

    // Step 5: Write the optimized iloc code
    write_iloc_file(bbg.instructions_, output_filepath);

    return 0;
}