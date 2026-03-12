#include <string>
#include <unordered_map>
#include <vector>

#include "cfg/basic_block.hpp"

struct ValueTableEntry {
    int value_number;
    std::string subsumed_by {};
    std::vector<std::string> subsumes {};
};

class LocalValueNumbering {
private:
    int value_number_counter_ = 0;
    // maps value number to constant value
    std::unordered_map<int, int> constant_table_;
    // maps expression string to l-value
    std::unordered_map<std::string, std::string> expression_table_;
    // maps l-value to ValueTableEntry (value number, subsumed_by, subsumes)
    std::unordered_map<std::string, ValueTableEntry> value_table_;

    // Helper function that replaces the instruction's source registers with their subsuming
    // registers
    void apply_subsume(Instruction& instruction);

    // Helper function to create a subsume relationship where lvalue is subsumed by rvalue
    void subsume(const std::string& lvalue, const std::string& rvalue);

    // Helper function to finds the canonical register for the given register by following subsume
    // chains
    std::string get_canonical_register(const std::string& reg);

    // Helper function to remove any subsume relationship for the given l-value
    void remove_subsume(const std::string& lvalue);

    // Helper function to get value number of the given register name from the value table (if
    // found). If not found create a new value number, insert into value table and return it. Also,
    // if it is a constant, insert into constant table.
    int value_number(std::string name);

    // Helper function to set the value number for a given l-value in the value table to the
    // provided value number (if l-value exists). If not, create a new entry then set value number.
    void set_value_number(const std::string& lvalue, int value_number);

    // Helper function to create a expression key of form
    // "min(operand1_vn,operand2_vn),opcode,max(operand1_vn,operand2_vn)" for commutative opcodes
    // and of form "operand1_vn,opcode,operand2_vn" for non-commutative opcodes
    std::string create_expression_key(const Instruction& instruction);

    // Helper function to perform arithmetic operation for constant folding
    int perform_arithmetic_operation(const std::string& opcode, int operand1, int operand2);

    // Helper function to perform comparison operation for constant folding
    int perform_comparison_operation(const std::string& opcode, int operand1, int operand2);

    // Optimizing function to combine comparison and test instructions into single comparison
    // instruction. Example: comp followed by testle to become a single cmp_LE instruction.
    void compress_comparison_and_test_instructions(
        BasicBlock& block, std::vector<Instruction>& instructions);

    // Optimizing function to:
    // 1. Remove any instructions whose lvalue is redefined again but was never used since the last
    // definition in the basic block
    // 2. Remove any instruction that is defined but never used later in the basic block when the
    // last instruction in the basic block is ret or iret or fret (i.e., no fallthrough)
    void perform_local_dead_code_elimination(
        BasicBlock& block, std::vector<Instruction>& instructions);

    // Optimizing function to convert arithmetic opcodes to their immediate forms, when loadI
    // immediately followed by one of add, sub, mult, lshift or rshift using its lvalue as one of
    // the source operands
    void convert_arithmetic_opcode_to_immediate_form(
        BasicBlock& block, std::vector<Instruction>& instructions);

    // Helper function to reset all per-basic-block states from previous block optimization
    void reset_per_basic_block_state() {
        value_number_counter_ = 0;
        constant_table_.clear();
        expression_table_.clear();
        value_table_.clear();
    }

public:
    void optimize_basic_block(BasicBlock& block, std::vector<Instruction>& instructions);
};