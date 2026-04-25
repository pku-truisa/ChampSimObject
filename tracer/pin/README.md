# Intel PIN tracer

The included PIN tool `champsim_tracer.cpp` can be used to generate new traces.
It has been tested (April 2022) using PIN 3.22.

## Download and install PIN

Download the source of PIN from Intel's website, then build it in a location of your choice.

    wget https://software.intel.com/sites/landingpage/pintool/downloads/pin-3.22-98547-g7a303a835-gcc-linux.tar.gz
    tar zxf pin-3.22-98547-g7a303a835-gcc-linux.tar.gz
    cd pin-3.22-98547-g7a303a835-gcc-linux/source/tools
    make
    export PIN_ROOT=/your/path/to/pin

## Building the tracer

The provided makefile will generate `obj-intel64/champsim_tracer.so`.

    make
    $PIN_ROOT/pin -t obj-intel64/champsim_tracer.so -- <your program here>

The tracer has several options you can set:

### Basic Options
```
-o
Specify the output file for your trace.
The default is default_trace.champsim

-s <number>
Specify the number of instructions to skip in the program before tracing begins.
The default value is 0.

-t <number>
The number of instructions to trace, after -s instructions have been skipped.
The default value is 1,000,000.
```

### Simpoint Mode (Multi-Segment Tracing)
```
-p <simpoint_file>
Enable simpoint-based multi-segment tracing.
Specify a file where each line contains a segment marker (in billions of instructions).
Each segment will be traced to a separate output file.
Output files are named: <base_name><segment_value>B.trace
```

For example, you could trace 200,000 instructions of the program ls, after skipping the first 100,000 instructions, with this command:

    pin -t obj/champsim_tracer.so -o traces/ls_trace.champsim -s 100000 -t 200000 -- ls

### Simpoint Mode Example

Create a simpoint file (e.g., `simpoint.out`) with segment markers:
```
100
150
200
250
```

This defines 4 segments starting at 100 billion, 150 billion, 200 billion, and 250 billion instructions respectively.

Run the tracer with simpoint mode:

    pin -t obj-intel64/champsim_tracer.so -p simpoint.out -o champsim.trace -- ./my_application

This will generate the following trace files:
- `champsim100B.trace` (from instruction 100B onwards)
- `champsim150B.trace` (from instruction 150B onwards)
- `champsim200B.trace` (from instruction 200B onwards)
- `champsim250B.trace` (from instruction 250B onwards)

Each segment file contains all instructions from its start point until the next segment begins or the program exits.

Traces created with the champsim_tracer.so are approximately 64 bytes per instruction, but they generally compress down to less than a byte per instruction using xz compression.

