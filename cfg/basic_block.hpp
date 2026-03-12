#pragma once

#include <string>
#include <vector>

struct Instruction {
    int id;
    std::string opcode; // stores opcode or pseudo-op depending or nop (in case of label)
    std::vector<std::string> source;
    std::vector<std::string> target;
    std::string label {};
    bool is_pseudo_ops { false }; // example: .data .text .frame
    bool deleted { false }; // if instruction is to be deleted during optimization toggle this to
                            // true then skip the instruction when recreating the iLOC
};

struct BasicBlock {
    int id;
    // name of the procedure this block belongs to
    std::string procedure_name;
    // Both indices are inclusive
    int start_idx;
    int end_idx;
};