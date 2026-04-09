CXX = g++
CXXFLAGS = -std=c++11 -O2 -Wall

TARGET = cache_sim
SRCS = main.cpp memory_hierarchy.cpp repl_policy.cpp prefetcher.cpp
OBJS = $(SRCS:.cpp=.o)

L1_KB = 32
ASSOC = 8
BLOCK = 64
L1_LAT = 1
MEM_LAT = 100

SANITY_TRACE = trace_sanity.txt
MY_TRACE = my_trace.txt

TASK3_ASSOC ?= 8
TASK3_BLOCK ?= 64
TASK3_L1_POLICY ?= BIP
TASK3_L1_PREFETCH ?= Stride
TASK3_L2_POLICY ?= SRRIP
TASK3_L2_PREFETCH ?= None

.PHONY: all clean run task1 task2 task3 trace_gen

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

run: task1

task1: $(TARGET)
	./$(TARGET) $(SANITY_TRACE) $(L1_KB) $(ASSOC) $(BLOCK) $(L1_LAT) $(MEM_LAT)

task2: $(TARGET)
	./$(TARGET) $(SANITY_TRACE) $(L1_KB) $(ASSOC) $(BLOCK) $(L1_LAT) $(MEM_LAT) --enable-l2

task3: $(TARGET)
	./$(TARGET) $(MY_TRACE) $(L1_KB) $(TASK3_ASSOC) $(TASK3_BLOCK) $(L1_LAT) $(MEM_LAT) $(TASK3_L1_POLICY) $(TASK3_L1_PREFETCH) --enable-l2 $(TASK3_L2_POLICY) $(TASK3_L2_PREFETCH)

trace_gen:
	$(CXX) $(CXXFLAGS) -o trace_generator/workload_gen trace_generator/workload_gen.cpp
