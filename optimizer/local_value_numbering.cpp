#include "optimizer/local_value_numbering.hpp"
#include "opcode_info.hpp"

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

void LocalValueNumbering::apply_subsume(Instruction& instruction) {
    for (std::string& src : instruction.source) {
        if (value_table_.contains(src)) {
            std::string subsumer = get_canonical_register(src);
            if (!subsumer.empty()) {
                src = subsumer;
            }
        }
    }
}

std::string LocalValueNumbering::get_canonical_register(const std::string& reg) {
    if (!value_table_.contains(reg)) {
        return reg;
    }

    std::string canonical_reg = reg;
    while (
        value_table_.contains(canonical_reg) && !value_table_[canonical_reg].subsumed_by.empty()) {
        canonical_reg = value_table_[canonical_reg].subsumed_by;
    }
    return canonical_reg;
}

int LocalValueNumbering::value_number(std::string name) {
    if (value_table_.contains(name)) {
        return value_table_[name].value_number;
    } else {
        int value_number = value_number_counter_++;
        ValueTableEntry entry;
        entry.value_number = value_number;
        value_table_[name] = entry;

        // If name is a constant (i.e., a number), insert into constant table as well
        try {
            int constant_value = std::stoi(name);
            constant_table_[value_number] = constant_value;
        } catch (const std::invalid_argument& e) {
            // Not a constant, do nothing
        }
        return value_number;
    }
}

void LocalValueNumbering::set_value_number(const std::string& lvalue, int value_number) {
    if (value_table_.contains(lvalue)) {
        value_table_[lvalue].value_number = value_number;
    } else {
        ValueTableEntry entry;
        entry.value_number = value_number;
        value_table_[lvalue] = entry;
    }
}

void LocalValueNumbering::subsume(const std::string& lvalue, const std::string& rvalue) {
    remove_subsume(lvalue);
    std::string canonical_rvalue = get_canonical_register(rvalue);
    value_table_[lvalue].subsumed_by = canonical_rvalue;
    value_table_[canonical_rvalue].subsumes.push_back(lvalue);
}

void LocalValueNumbering::remove_subsume(const std::string& lvalue) {
    if (!value_table_.contains(lvalue)) {
        return;
    }

    auto& entry = value_table_[lvalue];

    // Case 1: if lvalue was subsumed by another register, remove lvalue from that register's
    // subsumes list
    if (!entry.subsumed_by.empty()) {
        auto& subsumers_subsumes = value_table_[entry.subsumed_by].subsumes;
        std::erase(subsumers_subsumes, lvalue);
        entry.subsumed_by.clear();
    }

    // Case 2: if lvalue subsumes other registers, update those registers to be subsumed by no one
    for (auto& subsumed : entry.subsumes) {
        if (value_table_.contains(subsumed)) {
            value_table_[subsumed].subsumed_by.clear();
        }
    }

    entry.subsumes.clear();
}

std::string LocalValueNumbering::create_expression_key(const Instruction& instruction) {
    bool commutative = COMMUTATIVE_OPCODES.contains(instruction.opcode);

    std::string key = instruction.opcode;
    int operand1_vn = value_number(instruction.source[0]);

    // Handle opcode with single source like "not"
    if (instruction.source.size() == 1) {
        key = std::to_string(operand1_vn) + "," + instruction.opcode;
        return key;
    }

    int operand2_vn = value_number(instruction.source[1]);

    if (commutative) {
        int min_vn = std::min(operand1_vn, operand2_vn);
        int max_vn = std::max(operand1_vn, operand2_vn);
        key = std::to_string(min_vn) + "," + instruction.opcode + "," + std::to_string(max_vn);
    } else {
        key = std::to_string(operand1_vn) + "," + instruction.opcode + ","
            + std::to_string(operand2_vn);
    }
    return key;
}

int LocalValueNumbering::perform_arithmetic_operation(
    const std::string& opcode, int operand1, int operand2 = 0) {
    if (opcode == "add" || opcode == "addI") {
        return operand1 + operand2;
    } else if (opcode == "sub" || opcode == "subI") {
        return operand1 - operand2;
    } else if (opcode == "mult" || opcode == "multI") {
        return operand1 * operand2;
    } else if (opcode == "lshift" || opcode == "lshiftI") {
        return operand1 << operand2;
    } else if (opcode == "rshift" || opcode == "rshiftI") {
        return operand1 >> operand2;
    } else if (opcode == "mod") {
        if (operand2 == 0) {
            throw std::invalid_argument("Mod by zero in perform_arithmetic_operation");
        }
        return operand1 % operand2;
    } else if (opcode == "and") {
        return operand1 & operand2;
    } else if (opcode == "or") {
        return operand1 | operand2;
    } else if (opcode == "not") {
        return ~operand1;
    } else {
        throw std::invalid_argument("Unsupported opcode for arithmetic operation: " + opcode);
    }
}

int LocalValueNumbering::perform_comparison_operation(
    const std::string& opcode, int operand1, int operand2) {
    if (opcode == "testeq" || opcode == "cmp_EQ" || opcode == "cbr_EQ") {
        return (operand1 == operand2) ? 1 : 0;
    } else if (opcode == "testne" || opcode == "cmp_NE" || opcode == "cbr_NE") {
        return (operand1 != operand2) ? 1 : 0;
    } else if (opcode == "testlt" || opcode == "cmp_LT" || opcode == "cbr_LT") {
        return (operand1 < operand2) ? 1 : 0;
    } else if (opcode == "testle" || opcode == "cmp_LE" || opcode == "cbr_LE") {
        return (operand1 <= operand2) ? 1 : 0;
    } else if (opcode == "testgt" || opcode == "cmp_GT" || opcode == "cbr_GT") {
        return (operand1 > operand2) ? 1 : 0;
    } else if (opcode == "testge" || opcode == "cmp_GE" || opcode == "cbr_GE") {
        return (operand1 >= operand2) ? 1 : 0;
    } else {
        throw std::invalid_argument("Unsupported opcode for comparison operation: " + opcode);
    }
}

void LocalValueNumbering::optimize_basic_block(
    BasicBlock& block, std::vector<Instruction>& instructions) {

    // NOTE: Float handling is incomplete/simplified in this implementation. Few implementation
    // notes regarding float handling:
    // 1. Float values in registers are treated like any other register values.
    // 2. Float constants (e.g. .float_const_0) are not parsed or stored in constant table. They are
    // handled as symbolic registers names not numeric values. So constant folding and stuffs only
    // work for the integer constants.

    reset_per_basic_block_state();

    // NOTE: when instruction can be deleted, its "deleted" attribute is set to true but instruction
    // is not removed from instructions vector or block. Removal of such instructions is
    // handled later when recreating the iLOC from optimized instructions by not including
    // instructions marked as deleted while printing.

    for (int i = block.start_idx; i <= block.end_idx; i++) {
        // Pseduo-ops are not in the basic block so need to worry about them.
        // Levels can be treated like any other instruction since they have opcode, source and
        // target operands and label itself is stored in different attribute "instruction.label".
        Instruction& instruction = instructions[i];

        // Skip instructions without source and target operands like: ret, and stack operations
        // like pop, stadd
        // NOTE: Opcodes like: fread, iread, jump don't have source operands but have target
        // operands, but iLOC parser is implemented such that these target operands are stored as
        // source operands because "=>" and "->" (source to target indicators) are missing in these
        // instructions. So, these opcodes will be skipped below in another check where empty target
        // operands are checked. Due to this implementation decision APPLY_SUBSUME is applied on
        // target of opcode like: fread, iread and jump which is okay (I guess).
        // TODO: Need to verify the last statement above and might need to reconsider the design
        // decision.
        if (instruction.source.empty()) {
            continue;
        }

        // TODO: Even though iLOC has opcodes that have more than one target operand (storeAI,
        // storeAO, fstoreAI, fstoreAO), skipping those for now. Haven't seen those in iLOC test
        // files but in future might need to handle those opcodes in optimization. Eventhough more
        // than one target operands aren't handled here, parser does parse more than one target
        // operands correctly.
        if (instruction.target.size() > 1) {
            continue;
        }

        // Skip jumpI as it has no source operands and only target operand is label
        if (instruction.opcode == "jumpI") {
            continue;
        }

        // Need to do this before processing the instructions.
        // APPLY_SUBSUME is also applied to instructions that doesn't have source but do have target
        // (except jumpI) which were not removed above due to design decision of treating target as
        // source for some opcodes. Real targets (here source) of this opcodes contains register
        // names to which subsume relationships could be applied.
        apply_subsume(instruction);

        // Skip instructions that uses targets as memory addresses, not definitions like: storeAI,
        // storeAO, fstoreAI, fstoreAO, store, fread, iread after applying subsume to their target
        if (OPCODES_USING_TARGET.contains(instruction.opcode)) {
            continue;
        }

        std::string lvalue = instruction.target.empty() ? "" : instruction.target[0];

        // Skip instructions without target like: fwrite, iwrite, swrite, iret, fret, push, pushr.
        // However, APPLY_SUBSUME is applied on source operands above.
        // Note: Even though Opcodes (fread, iread) have target operands, they are skipped above
        // when checking for missing target for reason mentioned above.
        if (lvalue.empty()) {
            continue;
        }

        // TODO: Do I need to handle: call (skipped in missing target check after
        // APPLY_SUBSUME), icall and fcall opcode (handled below like other instructions with
        // lvalue) specially?

        if (instruction.opcode == "loadI") {
            std::string rvalue = instruction.source[0];
            int rvalue_vn = value_number(rvalue);
            if (value_table_.contains(lvalue) && value_table_[lvalue].value_number == rvalue_vn) {
                instruction.deleted = true;
            } else {
                set_value_number(lvalue, rvalue_vn);
            }
            continue;
        }

        // i2i, f2i, i2f, f2f opcodes
        if (MOVE_OPCODES.contains(instruction.opcode)) {
            std::string rvalue = instruction.source[0];
            int rvalue_vn = value_number(rvalue);
            if (value_table_.contains(lvalue) && value_table_[lvalue].value_number == rvalue_vn) {
                instruction.deleted = true;
                continue;
            }

            remove_subsume(lvalue);
            set_value_number(lvalue, rvalue_vn);
            if (constant_table_.contains(rvalue_vn)) {
                instruction.opcode = "loadI";
                instruction.source.clear();
                instruction.source.push_back(std::to_string(constant_table_[rvalue_vn]));
            } else {
                subsume(lvalue, rvalue);
            }
            continue;
        }

        // Handle opcodes other than loadI and move opcodes

        int operand1_vn = value_number(instruction.source[0]);
        int operand2_vn = -1;
        if (instruction.source.size() > 1) {
            operand2_vn = value_number(instruction.source[1]);
        }

        // constant folding for arithmetic operations (add, sub, mult, and, or etc.) when all source
        // operands are constants
        if (ARITHMETIC_OPCODES.contains(instruction.opcode) && constant_table_.contains(operand1_vn)
            && (operand2_vn == -1 || constant_table_.contains(operand2_vn))) {
            try {
                int result;
                if (operand2_vn == -1) {
                    // Handle unary operation "not"
                    result = perform_arithmetic_operation(
                        instruction.opcode, constant_table_[operand1_vn]);
                } else {
                    result = perform_arithmetic_operation(instruction.opcode,
                        constant_table_[operand1_vn], constant_table_[operand2_vn]);
                }

                std::string result_str = std::to_string(result);
                instruction.opcode = "loadI";
                instruction.source.clear();
                instruction.source.push_back(result_str);
                remove_subsume(lvalue);
                set_value_number(lvalue, value_number(result_str));
                continue;
            } catch (const std::invalid_argument& e) {
                // Divide by zero or mod by zero during constant folding.
                // Skip constant folding for this instruction and fall through to normal expression
                // handling below.
            }
        }

        // constant folding for "comp" opcode followed by test opcode (testeq, testgt) when both
        // source operands are constants.
        // Need to handle this separately because unlike other comparison operations, operation to
        // be performed by"comp" depends on following test opcode
        if (instruction.opcode == "comp" && constant_table_.contains(operand1_vn)
            && constant_table_.contains(operand2_vn)) {
            std::string next_instruction_opcode = instructions[i + 1].opcode;

            if (TEST_OPCODES.contains(next_instruction_opcode)) {
                int result = perform_comparison_operation(next_instruction_opcode,
                    constant_table_[operand1_vn], constant_table_[operand2_vn]);

                instruction.deleted = true;
                instructions[i + 1].opcode = "loadI";
                instructions[i + 1].source.clear();
                instructions[i + 1].source.push_back(std::to_string(result));
                // No need to call remove_subsume(), value_number() or set_value_number() for lvalue
                // because this loadI will be handled in next iteration
                continue;
            }
            // else fall through to normal expression handling below
        }

        // constant folding for comparison operations (cmp_EQ, cmp_NE, cmp_LT etc.) except "comp"
        // (see previous if-condition) when both source operands are constants
        if (COMPARISON_OPCODES.contains(instruction.opcode) && constant_table_.contains(operand1_vn)
            && constant_table_.contains(operand2_vn)) {
            int result = perform_comparison_operation(
                instruction.opcode, constant_table_[operand1_vn], constant_table_[operand2_vn]);

            std::string result_str = std::to_string(result);
            instruction.opcode = "loadI";
            instruction.source.clear();
            instruction.source.push_back(result_str);
            remove_subsume(lvalue);
            set_value_number(lvalue, value_number(result_str));
            continue;
        }

        // Delete cbr if source is constant zero: branch will not be taken (fall through)
        if (instruction.opcode == "cbr" && constant_table_.contains(operand1_vn)
            && constant_table_[operand1_vn] == 0) {
            // cbr ends the basic block, so checking if it can be deleted or not is enough
            instruction.deleted = true;
            continue;
        }

        // Delete cbrne if source is constant non-zero: branch will not be taken (fall through)
        if (instruction.opcode == "cbrne" && constant_table_.contains(operand1_vn)
            && constant_table_[operand1_vn] != 0) {
            // cbrne ends the basic block, so checking if it can be deleted or not is enough
            instruction.deleted = true;
            continue;
        }

        // Handle conditional branches with two source operands like cbr_LT, cbr_EQ etc.
        if (CONDITIONAL_BRANCH_OPCODES_WITH_TWO_SOURCES.contains(instruction.opcode)
            && constant_table_.contains(operand1_vn) && constant_table_.contains(operand2_vn)) {
            // conditional branch ends the basic block, so checking if it can be deleted or not is
            // enough
            int result = perform_comparison_operation(
                instruction.opcode, constant_table_[operand1_vn], constant_table_[operand2_vn]);
            if (result == 0) {
                instruction.deleted = true;
                continue;
            }
            // else fall through to normal expression handling below
        }

        // Normal expression handling for other instructions that didn't qualify for any special
        // handling above
        std::string expression_key = create_expression_key(instruction);

        // Handle redundant expressions
        if (expression_table_.contains(expression_key)) {
            std::string expression_lvalue = expression_table_[expression_key];
            int expression_lvalue_vn = value_number(expression_lvalue);
            if (value_table_[lvalue].value_number == expression_lvalue_vn) {
                instruction.deleted = true;
                continue;
            }

            // For now, using i2i for all cases but might need to handle float moves too
            instruction.opcode = "i2i";
            instruction.source.clear();
            std::string canonical_expression_reg = get_canonical_register(expression_lvalue);
            instruction.source.push_back(canonical_expression_reg);
            remove_subsume(lvalue);
            set_value_number(lvalue, expression_lvalue_vn);
            subsume(lvalue, canonical_expression_reg);
            continue;
        }

        // Handle new expression

        // Propagate Constants: cases of all operands being constants are already handled above
        // during constant folding. Here, we handle cases like "a + 0 = a" or "a * 1 = a" for
        // addition where one of the operands is a constant OR neither of the source operand are
        // constants but always produce constant result or non-constant source operand as result.

        bool operand1_is_constant = constant_table_.contains(operand1_vn);
        bool operand2_is_constant = constant_table_.contains(operand2_vn);

        // Handle cases where result is always one of the operands
        if (
            // Case: a + 0 or 0 + a  = a
            (MATH_ADD_OPCODES.contains(instruction.opcode)
                && ((operand1_is_constant && constant_table_[operand1_vn] == 0)
                    || (operand2_is_constant && constant_table_[operand2_vn] == 0)))
            ||
            // Case: a * 1 or 1 * a = a
            (MATH_MULT_OPCODES.contains(instruction.opcode)
                && ((operand1_is_constant && constant_table_[operand1_vn] == 1)
                    || (operand2_is_constant && constant_table_[operand2_vn] == 1)))
            ||
            // Case: a << 0 = a
            (MATH_LSHIFT_OPCODES.contains(instruction.opcode)
                && (operand2_is_constant && constant_table_[operand2_vn] == 0))
            ||
            // Case: a >> 0 = a
            (MATH_RSHIFT_OPCODES.contains(instruction.opcode)
                && (operand2_is_constant && constant_table_[operand2_vn] == 0))
            ||
            // Case: a & a = a
            (instruction.opcode == "and" && operand1_vn == operand2_vn) ||
            // Case: a | a = a
            (instruction.opcode == "or" && operand1_vn == operand2_vn) ||
            // Case: a / 1 = a
            (instruction.opcode == "fdiv"
                && (operand2_is_constant && constant_table_[operand2_vn] == 1))
            ||
            // Case: a - 0  = a
            (MATH_SUB_OPCODES.contains(instruction.opcode)
                && (operand2_is_constant && constant_table_[operand2_vn] == 0)))
        // CAUTION: a - 0 != 0 - a
        {
            // For now, using i2i for all cases but might need to handle float moves too
            instruction.opcode = "i2i";
            // Saving original sources before clearing because in next step these values are pushed
            // back into the source vector
            std::string original_src0 = instruction.source[0];
            std::string original_src1 = instruction.source[1];
            instruction.source.clear();
            remove_subsume(lvalue);

            // Determine which operand is the identity element and which is the result
            // For operations like a + 0, a - 0, a * 1, a / 1: result is the non-identity operand
            if (operand1_is_constant
                && (constant_table_[operand1_vn] == 0 || constant_table_[operand1_vn] == 1)) {
                // operand1 is 0 or 1, result is operand2
                std::string canonical_src1 = get_canonical_register(original_src1);
                instruction.source.push_back(canonical_src1);
                set_value_number(lvalue, operand2_vn);
                subsume(lvalue, canonical_src1);
            } else {
                // operand2 is 0 or 1, or both operands are same (a & a, a | a)
                // result is operand1
                std::string canonical_src0 = get_canonical_register(original_src0);
                instruction.source.push_back(canonical_src0);
                set_value_number(lvalue, operand1_vn);
                subsume(lvalue, canonical_src0);
            }
            continue;
        }

        // Handle cases where result is always zero
        if (
            // Case: a * 0 or 0 * a = 0
            (MATH_MULT_OPCODES.contains(instruction.opcode)
                && ((operand1_is_constant && constant_table_[operand1_vn] == 0)
                    || (operand2_is_constant && constant_table_[operand2_vn] == 0)))
            ||
            // Case: a - a = 0
            (MATH_SUB_OPCODES.contains(instruction.opcode) && operand1_vn == operand2_vn) ||
            // Case: a & 0 = 0 or 0 & a = 0
            (instruction.opcode == "and"
                && ((operand1_is_constant && constant_table_[operand1_vn] == 0)
                    || (operand2_is_constant && constant_table_[operand2_vn] == 0)))) {
            instruction.opcode = "loadI";
            instruction.source.clear();
            instruction.source.push_back("0");
            remove_subsume(lvalue);
            set_value_number(lvalue, value_number("0"));
            continue;
        }

        // Handle cases where result is always one
        if (
            // Case: a / a = 1, where a != 0
            (instruction.opcode == "fdiv" && operand1_vn == operand2_vn
                && (operand1_is_constant && constant_table_[operand1_vn] != 0))
            ||
            // Case: a | 1 = 1 or 1 | a = 1
            (instruction.opcode == "or"
                && ((operand1_is_constant && constant_table_[operand1_vn] == 1)
                    || (operand2_is_constant && constant_table_[operand2_vn] == 1)))) {
            instruction.opcode = "loadI";
            instruction.source.clear();
            instruction.source.push_back("1");
            remove_subsume(lvalue);
            set_value_number(lvalue, value_number("1"));
            continue;
        }

        if (BRANCH_OPS.contains(instruction.opcode)) {
            // If branch instructions are not already handled above, skip them here as they are the
            // last instruction in the basic block and no further processing is needed.
            continue;
        }

        // All other new expressions
        expression_table_[expression_key] = lvalue;
        set_value_number(lvalue, value_number_counter_++);
    }

    // check if next instruction is add, sub or mult, if yes, mark this instruction as
    // deleted then convert next instruction from add to addI, sub to subI or mult to multI
    // respectively.
    // NOTE: This optimization (cheap trick) isn't a good optimization because the loadI
    // might be used elsewhere. But for now assuming that loadI immediately followed by add,
    // sub, mult, lshift or rshift using its lvalue is safe to optimize and convert to addI, subI,
    // multI, lshiftI or rshiftI respectively.
    convert_arithmetic_opcode_to_immediate_form(block, instructions);

    // Combine comparison and test instructions into single comparison instruction
    // Example: comp followed by testle to cmp_LE
    compress_comparison_and_test_instructions(block, instructions);

    // Things optimized by this function:
    // 1. Remove any instructions whose lvalue is redefined again and was never used since the last
    // definition in the basic block.
    // 2. If the last instruction in the basic block is ret or iret or fret, then there is no
    // fallthrough so any instruction that defines a register but that register is never used later
    // in the basic block can be removed.
    perform_local_dead_code_elimination(block, instructions);
}

void LocalValueNumbering::compress_comparison_and_test_instructions(
    BasicBlock& block, std::vector<Instruction>& instructions) {

    for (int i = block.start_idx; i < block.end_idx; i++) {
        Instruction& instruction = instructions[i];

        if (instruction.opcode != "comp") {
            continue;
        }

        Instruction& next_instruction = instructions[i + 1];

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

void LocalValueNumbering::convert_arithmetic_opcode_to_immediate_form(
    BasicBlock& block, std::vector<Instruction>& instructions) {

    for (int i = block.start_idx; i < block.end_idx; i++) {
        Instruction& instruction = instructions[i];

        if (instruction.opcode != "loadI") {
            continue;
        }

        // Skip if source is a float constant (e.g., .float_const_0)
        if (instruction.source[0][0] == '.') {
            continue;
        }

        Instruction& next_instruction = instructions[i + 1];

        if (next_instruction.opcode != "add" && next_instruction.opcode != "sub"
            && next_instruction.opcode != "mult" && next_instruction.opcode != "lshift"
            && next_instruction.opcode != "rshift") {
            continue;
        }

        // If the second source operand of next instruction is the lvalue of the current loadI
        // instruction, perform the optimization
        std::string lvalue = instruction.target[0];
        if (next_instruction.source[1] != lvalue) {
            continue;
        }

        if (next_instruction.opcode == "add") {
            next_instruction.opcode = "addI";
        } else if (next_instruction.opcode == "sub") {
            next_instruction.opcode = "subI";
        } else if (next_instruction.opcode == "mult") {
            next_instruction.opcode = "multI";
        } else if (next_instruction.opcode == "lshift") {
            next_instruction.opcode = "lshiftI";
        } else if (next_instruction.opcode == "rshift") {
            next_instruction.opcode = "rshiftI";
        }
        // Replace the second source operand of next instruction with the constant
        // value from current loadI instruction
        next_instruction.source[1] = instruction.source[0];
        // Mark current loadI instruction as deleted
        instruction.deleted = true;
    }
}

void LocalValueNumbering::perform_local_dead_code_elimination(
    BasicBlock& block, std::vector<Instruction>& instructions) {

    // Remove any instructions whose lvalue is redefined again and was never used since the last
    // definition in the basic block.
    struct use_def_info {
        int last_def_idx = -1;
        int last_use_idx = -1;
    };

    std::unordered_map<std::string, use_def_info> reg_use_def_map;
    for (int i = block.start_idx; i <= block.end_idx; i++) {
        Instruction& instruction = instructions[i];

        // Skip instructions already marked deleted in previous pass
        if (instruction.deleted) {
            continue;
        }

        for (const auto& src : instruction.source) {
            // Due to design decision mentioned earlier, fread and iread have their target
            // stored as source. So, skip them here as they will be handled separately below.
            if (instruction.opcode == "fread" || instruction.opcode == "iread") {
                break;
            }
            reg_use_def_map[src].last_use_idx = i;
        }

        // Handle instructions that use target operands like fread, iread, store, storeAI,
        // storeAO, fstore, fstoreAI, fstoreAO
        if (OPCODES_USING_TARGET.contains(instruction.opcode)) {
            for (const auto& target : instruction.target) {
                reg_use_def_map[target].last_use_idx = i;
            }
            continue;
        }

        for (const auto& target : instruction.target) {
            // if target is empty or is a label skip
            if (target.empty() || target[0] == '.') {
                continue;
            }

            if (reg_use_def_map.contains(target)) {
                int last_def_idx = reg_use_def_map[target].last_def_idx;
                int last_use_idx = reg_use_def_map[target].last_use_idx;

                // >= to handle cases like add %vr1, %vr2 => %vr1 where %vr1 is redefined and used
                // in same line
                if (last_def_idx != -1 && last_def_idx >= last_use_idx) {
                    instructions[last_def_idx].deleted = true;
                }
            }
            reg_use_def_map[target].last_def_idx = i;
        }
    }

    // If the last instruction in the basic block is ret or iret or fret, then there is no
    // fallthrough so any instruction that defines a register but that register is never used later
    // in the basic block can be removed.
    Instruction& last_instruction = instructions[block.end_idx];
    if (last_instruction.opcode != "ret" && last_instruction.opcode != "iret"
        && last_instruction.opcode != "fret") {
        return;
    }

    for (int i = block.start_idx; i <= block.end_idx; i++) {
        Instruction& instruction = instructions[i];

        // Skip instructions already marked deleted in previous pass
        if (instruction.deleted) {
            continue;
        }

        if (instruction.target.empty()) {
            continue;
        }

        std::string target = instruction.target[0];
        if (reg_use_def_map.contains(target) && reg_use_def_map[target].last_use_idx == -1) {
            // use index = -1 means never used
            instruction.deleted = true;
        }
    }
}