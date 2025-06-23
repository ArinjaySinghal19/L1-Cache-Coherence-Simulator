# L1 Cache Simulator with MESI Protocol

This project implements a simulator for L1 caches in a multi-core processor system, featuring the MESI (Modified, Exclusive, Shared, Invalid) cache coherence protocol. The simulator models cache behavior, memory access patterns, and bus transactions between multiple processor cores.

## Features

- Multi-core L1 cache simulation
- MESI cache coherence protocol implementation
- Configurable cache parameters (set size, associativity, block size)
- Detailed statistics tracking (hits, misses, evictions, writebacks, etc.)
- Support for parallel application trace files
- Bus-based communication between cores
- Debug mode for detailed operation logging (can be enabled by setting debugmode=true in main.cpp)

## Project Structure

- `main.cpp`: Main simulation driver and trace file processing
- `cache.h/cache.cpp`: Cache implementation with MESI protocol
- `bus.h/bus.cpp`: Bus implementation for inter-core communication
- `Makefile`: Build system configuration
- `report.tex`: Project documentation and analysis
- Various trace files for testing different scenarios

## Building the Project

The project uses a Makefile for building. To compile:

```bash
make
```

This will create the `L1simulate` executable.

## Usage

The simulator can be run with various command-line options:

```bash
./L1simulate [options]
```

### Options:
- `-t <tracefile>`: Name of parallel application (e.g., app1)
- `-s <s>`: Number of set index bits
- `-E <E>`: Associativity
- `-b <b>`: Number of block bits
- `-o <outfile>`: Output file for logging
- `-h`: Print help message

### Example Runs:

1. Run with test traces (default parameters):
```bash
make run_test
```

2. Run with app1 traces:
```bash
make run_app1
```

3. Run MESI protocol tests:
```bash
make run_mesi_test
```

4. Run with custom parameters:
```bash
make run ARGS="-t test -s 2 -E 2 -b 4 -o output.log"
```

## Cache Parameters

- **Set Index Bits (-s)**: Determines the number of sets in the cache (2^s sets)
- **Associativity (-E)**: Number of ways in the set-associative cache
- **Block Bits (-b)**: Size of each cache block (2^b bytes)

## Output

The simulator generates detailed statistics including:
- Read and write counts
- Hit and miss counts
- Eviction counts
- Writeback counts
- Invalidation counts
- Bus traffic
- Execution cycles

## Documentation

The project includes a detailed report in LaTeX format. To generate the report:

```bash
make report
```

This will:
1. Compile the LaTeX report (`report.tex`) using pdflatex
2. Run pdflatex twice to ensure proper cross-references and table of contents
3. Generate a PDF file named `report.pdf`
4. Clean up intermediate LaTeX files

The report includes:
- Detailed explanation of the MESI protocol implementation
- Cache architecture and design decisions
- Performance analysis and results
- Flow diagrams of cache and bus operations
- Test cases and validation

## Cleaning Up

To remove all generated files:

```bash
make clean
```

This will remove:
- All object files (*.o)
- The executable (L1simulate)
- Log files (output.log, mesi_test.log)
- LaTeX generated files (report.pdf, report.aux, report.log, report.toc, report.out)

## Dependencies

- C++11 compatible compiler (g++ recommended)
- Make
- LaTeX (for report generation)
  - pdflatex
  - Required LaTeX packages:
    - graphicx (for including images)
    - float (for figure placement)
    - amsmath (for mathematical equations)
    - amssymb (for mathematical symbols)
    - geometry (for page layout)
    - inputenc (for UTF-8 encoding)
    - enumitem (for customizing lists)
  - Note: These packages are typically included in standard LaTeX distributions like TeX Live or MiKTeX

## License

This project is part of a course assignment (COL216_A3) and is intended for educational purposes.