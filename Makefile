CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++11 -O3

TARGET = L1simulate
SRCS = main.cpp cache.cpp bus.cpp
OBJS = $(SRCS:.cpp=.o)

.PHONY: all clean run run_test run_app1 run_mesi_test report

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Run with test traces (default parameters)
run_test: $(TARGET)
	./$(TARGET) -t test -s 2 -E 2 -b 4 -o output.log

# Run with app1 traces
run_app1: $(TARGET)
	./$(TARGET) -t app1 -s 2 -E 2 -b 4 -o output.log

# Run MESI protocol tests
run_mesi_test: $(TARGET)
	./$(TARGET) -t test_mesi -s 2 -E 2 -b 4 -o mesi_test.log

# Generic run target that can accept parameters
# Usage: make run ARGS="-t test -s 2 -E 2 -b 4 -o output.log"
run: $(TARGET)
	./$(TARGET) $(ARGS)

# LaTeX compilation target
report: report.tex
	pdflatex report.tex
	pdflatex report.tex  # Run twice for proper cross-references and TOC
	rm -f report.aux report.log report.toc report.out

# Add report to clean target
clean:
	rm -f $(OBJS) $(TARGET) output.log mesi_test.log 
	rm -f L1simulate
	rm -f report.aux report.log report.toc report.out report.fdb_latexmk report.fls report.synctex.gz