#include "ssa/ssa_deconstructor.hpp"

static std::string strip_subscript(const std::string& name) {
    if (!name.starts_with("%vr")) {
        return name;
    }
    auto pos = name.rfind('_');
    if (pos == std::string::npos) {
        return name;
    }
    return name.substr(0, pos);
}

void SSA_Deconstructor::deconstruct_ssa(std::vector<Instruction>& instructions) {

    // Strip SSA subscripts from all registers in all instructions
    for (auto& instr : instructions) {
        for (auto& src : instr.source) {
            src = strip_subscript(src);
        }
        for (auto& tgt : instr.target) {
            tgt = strip_subscript(tgt);
        }
    }
}