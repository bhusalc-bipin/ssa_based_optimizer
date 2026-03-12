# Makefile for SSA based optimizer (created using AI)

CXX = g++
CXXFLAGS = -std=c++23 -Wall -Wextra -I.
TARGET = ssa_optimizer
SRCS = main.cpp cfg/basic_block_generator.cpp cfg/cfg_generator.cpp analysis/dominance_analyzer.cpp analysis/liveness_analyzer.cpp ssa/ssa_constructor.cpp ssa/ssa_deconstructor.cpp optimizer/ssa_based_optimizer.cpp
OBJS = $(SRCS:.cpp=.o)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
	rm -rf output/

.PHONY: clean