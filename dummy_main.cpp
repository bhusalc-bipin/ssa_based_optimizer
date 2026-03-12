// dummy_main.cpp
#include "analysis/dominance_analyzer.hpp"
#include "cfg/basic_block_generator.hpp"
#include "cfg/cfg_generator.hpp"
#include "ssa/ssa_constructor.hpp"

#include <iostream>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <input_iloc_file>\n";
        return 1;
    }

    // Parse and build basic blocks
    BasicBlockGenerator bbg;
    bbg.parse_iloc_file(argv[1]);
    bbg.build_basic_blocks();

    // Print basic blocks
    std::cout << "=== Basic Blocks ===\n";
    for (const auto& block : bbg.blocks_) {
        std::cout << "Block " << block.id << " [procedure: " << block.procedure_name << ", instr "
                  << block.start_idx << "-" << block.end_idx << "]\n";
        for (int i = block.start_idx; i <= block.end_idx; i++) {
            const auto& instr = bbg.instructions_[i];
            std::cout << "  " << i + 1 << ": ";
            if (!instr.label.empty())
                std::cout << instr.label << ": ";
            std::cout << instr.opcode;
            for (const auto& s : instr.source)
                std::cout << " " << s;
            if (!instr.target.empty()) {
                std::cout << " =>";
                for (const auto& t : instr.target)
                    std::cout << " " << t;
            }
            std::cout << "\n";
        }
    }

    // Build CFG
    CFG_Generator cfg_gen;
    cfg_gen.build_cfg(bbg.blocks_, bbg.instructions_);

    // Print CFGs
    std::cout << "\n=== Control Flow Graphs ===\n";
    for (const auto& cfg : cfg_gen.cfgs_) {
        std::cout << "\nProcedure: " << cfg.procedure_name << "\n";
        std::cout << "Blocks: ";
        for (int id : cfg.block_ids)
            std::cout << id << " ";
        std::cout << "\n";

        std::cout << "\nEdges (successor lists):\n";

        // Print entry block edges
        auto it = cfg.successors.find(ENTRY_BLOCK_ID);
        if (it != cfg.successors.end()) {
            std::cout << "  ENTRY -> ";
            for (int s : it->second)
                std::cout << s << " ";
            std::cout << "\n";
        }

        // Print real block edges
        for (int bid : cfg.block_ids) {
            auto sit = cfg.successors.find(bid);
            if (sit != cfg.successors.end()) {
                std::cout << "  " << bid << " -> ";
                for (int s : sit->second) {
                    if (s == EXIT_BLOCK_ID)
                        std::cout << "EXIT ";
                    else
                        std::cout << s << " ";
                }
                std::cout << "\n";
            }
        }

        std::cout << "\nEdges (predecessor lists):\n";

        // Print real block predecessors
        for (int bid : cfg.block_ids) {
            auto pit = cfg.predecessors.find(bid);
            if (pit != cfg.predecessors.end()) {
                std::cout << "  " << bid << " <- ";
                for (int p : pit->second) {
                    if (p == ENTRY_BLOCK_ID)
                        std::cout << "ENTRY ";
                    else
                        std::cout << p << " ";
                }
                std::cout << "\n";
            }
        }

        // Print exit block predecessors
        auto eit = cfg.predecessors.find(EXIT_BLOCK_ID);
        if (eit != cfg.predecessors.end()) {
            std::cout << "  EXIT <- ";
            for (int p : eit->second)
                std::cout << p << " ";
            std::cout << "\n";
        }

        // Compute and print dominators
        Dominance_Analyzer da;
        da.perform_dominance_analysis(cfg);

        std::cout << "\nDominators:\n";
        for (int bid : cfg.block_ids) {
            std::cout << "  Dom(" << bid << ") = { ";
            for (int d : da.dominance_info[bid].dominators)
                std::cout << d << " ";
            std::cout << "}\n";
        }

        std::cout << "\nImmediate Dominators:\n";
        for (int bid : cfg.block_ids) {
            int idom = da.dominance_info[bid].immediate_dominator;
            std::cout << "  IDom(" << bid << ") = ";
            if (idom == -1)
                std::cout << "none (entry)";
            else
                std::cout << idom;
            std::cout << "\n";
        }

        std::cout << "\nDominance Frontiers:\n";
        for (int bid : cfg.block_ids) {
            std::cout << "  DF(" << bid << ") = { ";
            for (int f : da.dominance_info[bid].dominance_frontier)
                std::cout << f << " ";
            std::cout << "}\n";
        }

        // Construct SSA form
        SSA_Constructor ssa;
        ssa.construct_ssa_form(cfg, bbg.blocks_, bbg.instructions_, da.dominance_info);

        // Print SSA form: phi functions and renamed instructions per block
        std::cout << "\n=== SSA Form ===\n";
        for (int bid : cfg.block_ids) {
            std::cout << "\nBlock " << bid << ":\n";

            // Print phi functions for this block
            if (ssa.phi_functions.contains(bid)) {
                for (const auto& phi : ssa.phi_functions[bid]) {
                    std::cout << "  " << phi.target << " = phi(";
                    bool first = true;
                    for (const auto& [pred_id, arg] : phi.args) {
                        if (!first)
                            std::cout << ", ";
                        std::cout << arg << " [from block " << pred_id << "]";
                        first = false;
                    }
                    std::cout << ")\n";
                }
            }

            // Print renamed instructions
            const auto& block = bbg.blocks_[bid];
            for (int i = block.start_idx; i <= block.end_idx; i++) {
                const auto& instr = bbg.instructions_[i];
                std::cout << "  ";
                if (!instr.label.empty())
                    std::cout << instr.label << ": ";
                std::cout << instr.opcode;
                for (const auto& s : instr.source)
                    std::cout << " " << s;
                if (!instr.target.empty()) {
                    std::cout << " =>";
                    for (const auto& t : instr.target)
                        std::cout << " " << t;
                }
                std::cout << "\n";
            }
        }
    }

    return 0;
}