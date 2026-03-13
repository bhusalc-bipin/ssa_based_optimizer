#pragma once

#include <string>
#include <unordered_set>

inline const std::unordered_set<std::string> COMMUTATIVE_OPCODES = {
    "add",
    "addI",
    "fadd",
    "mult",
    "multI",
    "fmult",
    "and",
    "or",
};

inline const std::unordered_set<std::string> MOVE_OPCODES = {
    "i2i",
    "f2i",
    "i2f",
    "f2f",
};

inline const std::unordered_set<std::string> ARITHMETIC_OPCODES = {
    "add",
    "sub",
    "mult",
    "lshift",
    "rshift",
    "mod",
    "and",
    "or",
    "not",
    "addI",
    "subI",
    "multI",
    "lshiftI",
    "rshiftI",
    "fadd",
    "fsub",
    "fmult",
    "fdiv",
};

inline const std::unordered_set<std::string> PSEUDO_OPS = {
    ".data",
    ".text",
    ".frame",
    ".global",
    ".string",
    ".float",
};

inline const std::unordered_set<std::string> BRANCH_OPS = {
    "jumpI",
    "jump",
    "call",
    "icall",
    "fcall",
    "ret",
    "iret",
    "fret",
    "cbr",
    "cbrne",
    "cbr_LT",
    "cbr_LE",
    "cbr_GT",
    "cbr_GE",
    "cbr_EQ",
    "cbr_NE",
};

inline const std::unordered_set<std::string> TEST_OPCODES = {
    "testeq",
    "testne",
    "testlt",
    "testle",
    "testgt",
    "testge",
};

inline const std::unordered_set<std::string> CONDITIONAL_BRANCH_OPCODES_WITH_TWO_SOURCES = {
    "cbr_LT",
    "cbr_LE",
    "cbr_GT",
    "cbr_GE",
    "cbr_EQ",
    "cbr_NE",
};

inline const std::unordered_set<std::string> COMPARISON_OPCODES = {
    // "comp" is not included here as it is handled separately in LVN because operation depends on
    // following test opcode)
    "cmp_EQ",
    "cmp_NE",
    "cmp_LT",
    "cmp_LE",
    "cmp_GT",
    "cmp_GE",
};

inline const std::unordered_set<std::string> MATH_ADD_OPCODES = {
    "add",
    "addI",
    "fadd",
};

inline const std::unordered_set<std::string> MATH_MULT_OPCODES = {
    "mult",
    "multI",
    "fmult",
};

inline const std::unordered_set<std::string> MATH_SUB_OPCODES = {
    "sub",
    "subI",
    "fsub",
};

inline const std::unordered_set<std::string> MATH_LSHIFT_OPCODES = {
    "lshift",
    "lshiftI",
};

inline const std::unordered_set<std::string> MATH_RSHIFT_OPCODES = {
    "rshift",
    "rshiftI",
};

inline const std::unordered_set<std::string> OPCODES_USING_TARGET = {
    "store",
    "storeAI",
    "storeAO",
    "fstore",
    "fstoreAI",
    "fstoreAO",
    "iread",
    "fread",
};

inline const std::unordered_set<std::string> UNCONDITIONAL_BRANCH_OPCODES_MINUS_CALLS_AND_RETURNS
    = {
          "jumpI",
          "jump",
      };

inline const std::unordered_set<std::string> CONDITIONAL_BRANCH_OPCODES = {
    "cbr",
    "cbrne",
    "cbr_LT",
    "cbr_LE",
    "cbr_GT",
    "cbr_GE",
    "cbr_EQ",
    "cbr_NE",
};

inline const std::unordered_set<std::string> RETURN_OPCODES = {
    "ret",
    "iret",
    "fret",
};

inline const std::unordered_set<std::string> OPCODES_WITH_SIDE_EFFECT = {
    "load",
    "loadAI",
    "loadAO",
    "store",
    "storeAI",
    "storeAO",
    "fload",
    "floadAI",
    "floadAO",
    "fstore",
    "fstoreAI",
    "fstoreAO",
    "call",
    "icall",
    "fcall",
    "ret",
    "iret",
    "fret",
    "fread",
    "iread",
    "fwrite",
    "iwrite",
    "swrite",
};