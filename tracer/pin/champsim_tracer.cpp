/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*! @file
 *  This is an example of the PIN tool that demonstrates some basic PIN APIs
 *  and could serve as the starting point for developing your first PIN tool
 */

#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <sstream>

#include "../../inc/trace_instruction.h"
#include "pin.H"

using trace_instr_format_t = input_instr;

/* ================================================================== */
// Debug logging macro - only log when debug mode is enabled
/* ================================================================== */
#define DEBUG_LOG(msg) do { \
  if (enableDebugLog && debug_log.is_open()) { \
    debug_log << msg << std::endl; \
  } \
} while(0)

/* ================================================================== */
// Global variables
/* ================================================================== */

UINT64 instrCount = 0;

std::vector<std::ofstream*> segmentFiles; // All segment trace files opened at once
std::ofstream malloc_outfile;
std::ofstream debug_log; // Debug log file for PIN tool diagnostics
std::vector<trace_instr_format_t> malloc_traces;
std::vector<trace_instr_format_t> instr_buffer;
const size_t INSTR_BUFFER_SIZE = 16 * 1024 * 1024; // 16MB buffer

trace_instr_format_t curr_instr;

// Simpoint-related variables
std::vector<UINT64> simpointSegments; // Segment markers in billions of instructions
std::vector<std::string> segmentFileNames; // Output file names for each segment
UINT32 currentSegmentIndex = 0; // Current active segment index (1-based, 0 means not started)
bool useSimpointMode = false; // Flag to indicate if simpoint mode is active
bool enableDebugLog = false; // Debug logging flag - disable for performance

/* ===================================================================== */
// Command line switches
/* ===================================================================== */
KNOB<std::string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "champsim.trace", "specify file name for Champsim tracer output");

KNOB<std::string> KnobMallocOutputFile(KNOB_MODE_WRITEONCE, "pintool", "m", "malloc.trace", "specify file name for malloc trace output");

KNOB<std::string> KnobSimpointFile(KNOB_MODE_WRITEONCE, "pintool", "p", "", "specify simpoint file path (each line is a segment marker in billions of instructions)");

KNOB<UINT64> KnobSkipInstructions(KNOB_MODE_WRITEONCE, "pintool", "s", "0", "How many instructions to skip before tracing begins");

KNOB<UINT64> KnobTraceInstructions(KNOB_MODE_WRITEONCE, "pintool", "t", "0", "How many instructions to trace (0 for unlimited)");

KNOB<BOOL> KnobEnableDebug(KNOB_MODE_WRITEONCE, "pintool", "d", "0", "Enable debug logging (0=disabled for performance, 1=enabled for debugging)");

/*!
 *  Print out help message.
 */
INT32 Usage()
{
  std::cerr << "This tool creates a register and memory access trace" << std::endl
            << "Specify the output trace file with -o" << std::endl
            << "Specify simpoint file with -p (each line is a segment marker in billions)" << std::endl
            << "Specify the number of instructions to skip before tracing with -s" << std::endl
            << "Specify the number of instructions to trace with -t" << std::endl
            << "Enable debug logging with -d (0=disabled for performance, 1=enabled for debugging)" << std::endl
            << std::endl;

  std::cerr << KNOB_BASE::StringKnobSummary() << std::endl;

  return -1;
}

/* ===================================================================== */
// Utilities
/* ===================================================================== */

/*!
 *  Read simpoint file and initialize segment tracking
 */
BOOL ReadSimpointFile(const std::string& simpointFilePath, const std::string& baseOutputFile)
{
  std::ifstream simpointFile(simpointFilePath.c_str());
  if (!simpointFile.is_open()) {
    DEBUG_LOG("Error: Cannot open simpoint file: " << simpointFilePath);
    return FALSE;
  }

  std::string line;
  UINT32 segmentIndex = 0;
  
  while (std::getline(simpointFile, line)) {
    // Skip empty lines
    if (line.empty()) continue;
    
    // Parse the number (segment marker in billions)
    std::istringstream iss(line);
    UINT64 segmentValue;
    if (!(iss >> segmentValue)) {
      DEBUG_LOG("Warning: Skipping invalid line in simpoint file: " << line);
      continue;
    }
    
    // Convert billions to actual instruction count
    UINT64 segmentInstrCount = segmentValue * 1000000000ULL; // Multiply by 1 billion
    
    simpointSegments.push_back(segmentInstrCount);
    
    // Generate output filename: <base><segmentValue>B.trace
    // Remove .trace extension from base if present
    std::string baseName = baseOutputFile;
    size_t traceExtPos = baseName.find(".trace");
    if (traceExtPos != std::string::npos) {
      baseName = baseName.substr(0, traceExtPos);
    }
    
    std::ostringstream fileNameStream;
    fileNameStream << baseName << segmentValue << "B.trace";
    segmentFileNames.push_back(fileNameStream.str());
    
    segmentIndex++;
  }
  
  simpointFile.close();
  
  if (simpointSegments.empty()) {
    DEBUG_LOG("Error: No valid segments found in simpoint file");
    return FALSE;
  }
  
  DEBUG_LOG("Loaded " << simpointSegments.size() << " simpoint segments");
  for (UINT32 i = 0; i < simpointSegments.size(); i++) {
    DEBUG_LOG("  Segment " << i << ": " << segmentFileNames[i] 
              << " (starts at " << simpointSegments[i] << " instructions)");
  }
  
  return TRUE;
}

/*!
 *  Open ALL segment trace files at initialization
 */
BOOL OpenAllSegmentFiles()
{
  if (segmentFileNames.empty()) {
    DEBUG_LOG("Error: No segment files to open");
    return FALSE;
  }
  
  // Safety check: limit the number of simultaneously open files
  // Most systems have a file descriptor limit (typically 1024 or higher)
  const size_t MAX_OPEN_FILES = 500; // Conservative limit
  if (segmentFileNames.size() > MAX_OPEN_FILES) {
    DEBUG_LOG("Error: Too many segments (" << segmentFileNames.size() 
              << "). Maximum supported: " << MAX_OPEN_FILES);
    return FALSE;
  }
  
  for (size_t i = 0; i < segmentFileNames.size(); i++) {
    std::ofstream* newFile = new std::ofstream();
    newFile->open(segmentFileNames[i].c_str(), std::ios_base::binary | std::ios_base::trunc);
    
    if (!newFile->is_open()) {
      DEBUG_LOG("Error: Cannot open segment output file: " << segmentFileNames[i]);
      // Clean up already opened files
      for (size_t j = 0; j < i; j++) {
        if (segmentFiles[j]->is_open()) {
          segmentFiles[j]->close();
        }
        delete segmentFiles[j];
      }
      delete newFile;
      return FALSE;
    }
    
    segmentFiles.push_back(newFile);
    DEBUG_LOG("Opened segment file " << i << ": " << segmentFileNames[i]);
  }
  
  DEBUG_LOG("Successfully opened " << segmentFiles.size() << " segment files");
  return TRUE;
}

/*!
 *  Write malloc/calloc/realloc/free instruction to ALL segment files
 */
VOID WriteMallocToAllSegments(const trace_instr_format_t& instr)
{
  for (size_t i = 0; i < segmentFiles.size(); i++) {
    // Defensive check: validate file pointer before access
    if (segmentFiles[i] == nullptr) {
      DEBUG_LOG("Warning: segmentFiles[" << i << "] is null pointer, skipping");
      continue;
    }
    
    if (segmentFiles[i]->is_open()) {
      try {
        segmentFiles[i]->write(reinterpret_cast<const char*>(&instr), 
                              sizeof(trace_instr_format_t));
        if (!segmentFiles[i]->good()) {
          DEBUG_LOG("Error: Failed to write malloc instruction to segment " << i);
        } else {
          // Flush immediately to ensure data persistence for global events
          segmentFiles[i]->flush();
          if (!segmentFiles[i]->good()) {
            DEBUG_LOG("Error: Failed to flush malloc instruction to segment " << i);
          }
        }
      } catch (const std::exception& e) {
        DEBUG_LOG("Error: Exception writing malloc to segment " << i << ": " << e.what());
      }
    }
  }
}

/*!
 *  Switch to next segment (just updates index, all files already open)
 */
VOID SwitchToNextSegment()
{
  if (currentSegmentIndex >= segmentFiles.size()) {
    return; // No more segments
  }
  
  // Flush current segment buffer before switching
  // This ensures all instructions (including malloc) are written in correct order
  if (!instr_buffer.empty() && currentSegmentIndex > 0 && currentSegmentIndex <= segmentFiles.size()) {
    // Defensive check: validate file pointer before writing
    size_t fileIndex = currentSegmentIndex - 1;
    if (segmentFiles[fileIndex] != nullptr && segmentFiles[fileIndex]->is_open()) {
      try {
        segmentFiles[fileIndex]->write(
            reinterpret_cast<const char*>(&instr_buffer[0]), 
            instr_buffer.size() * sizeof(trace_instr_format_t));
        if (!segmentFiles[fileIndex]->good()) {
          DEBUG_LOG("Error: Failed to flush buffer for segment " << fileIndex);
        }
      } catch (const std::exception& e) {
        DEBUG_LOG("Error: Exception flushing buffer for segment " << fileIndex << ": " << e.what());
      }
    } else {
      DEBUG_LOG("Warning: Cannot flush buffer, segment file " << fileIndex 
                << " is null or not open");
    }
    instr_buffer.clear();
  }
  
  // Log before incrementing to avoid out-of-bounds access
  DEBUG_LOG("Switched to segment " << currentSegmentIndex 
            << ": " << segmentFileNames[currentSegmentIndex - 1]);
  
  currentSegmentIndex++;
}

/*!
 *  Check if we should switch to next segment
 */
BOOL ShouldSwitchSegment()
{
  if (!useSimpointMode || currentSegmentIndex >= simpointSegments.size()) {
    return FALSE;
  }
  
  // Check if we've reached the start of the next segment
  return instrCount >= simpointSegments[currentSegmentIndex];
}

/* ===================================================================== */
// Analysis routines
/* ===================================================================== */

// Malloc tracking functions
VOID MallocBefore(ADDRINT size, ADDRINT ip)
{
  malloc_outfile << "MALLOC_SIZE " << size << std::endl;
  if (!malloc_outfile.good()) {
    DEBUG_LOG("Error: Failed to write to malloc trace file");
  }
  
  trace_instr_format_t instr = {};
  instr.ip = (unsigned long long int)ip;
  instr.is_malloc = 1; // 1: malloc
  instr.source_memory[0] = size;
  malloc_traces.push_back(instr);
}

VOID MallocAfter(ADDRINT ret)
{
  malloc_outfile << "MALLOC_RET 0x" << std::hex << ret << std::dec << std::endl;
  if (!malloc_outfile.good()) {
    DEBUG_LOG("Error: Failed to write to malloc trace file");
  }
  
  if (!malloc_traces.empty()) {
    curr_instr = malloc_traces.back();
    curr_instr.destination_memory[0] = ret;
    WriteCurrentInstruction(); // Writes to ALL segment files for malloc
    malloc_traces.pop_back();
  } else {
    DEBUG_LOG("Warning: MallocAfter called without matching MallocBefore");
  }
}

VOID FreeBefore(ADDRINT ptr, ADDRINT ip)
{
  malloc_outfile << "FREE 0x" << std::hex << ptr << std::dec << std::endl;
  if (!malloc_outfile.good()) {
    DEBUG_LOG("Error: Failed to write to malloc trace file");
  }
  
  curr_instr = {};
  curr_instr.ip = (unsigned long long int)ip;
  curr_instr.is_malloc = 4; // 4: free
  curr_instr.source_memory[0] = ptr;
  WriteCurrentInstruction(); // Writes to ALL segment files for free
}

VOID CallocBefore(ADDRINT nmemb, ADDRINT size, ADDRINT ip)
{
  malloc_outfile << "CALLOC_SIZE " << nmemb << " " << size << std::endl;
  if (!malloc_outfile.good()) {
    DEBUG_LOG("Error: Failed to write to malloc trace file");
  }
  
  trace_instr_format_t instr = {};
  instr.ip = (unsigned long long int)ip;
  instr.is_malloc = 2; // 2: calloc
  instr.source_memory[0] = nmemb * size;
  malloc_traces.push_back(instr);
}

VOID CallocAfter(ADDRINT ret)
{
  malloc_outfile << "CALLOC_RET 0x" << std::hex << ret << std::dec << std::endl;
  if (!malloc_outfile.good()) {
    DEBUG_LOG("Error: Failed to write to malloc trace file");
  }
  
  if (!malloc_traces.empty()) {
    curr_instr = malloc_traces.back();
    curr_instr.destination_memory[0] = ret;
    WriteCurrentInstruction(); // Writes to ALL segment files for calloc
    malloc_traces.pop_back();
  } else {
    DEBUG_LOG("Warning: CallocAfter called without matching CallocBefore");
  }
}

VOID ReallocBefore(ADDRINT ptr, ADDRINT size, ADDRINT ip)
{
  malloc_outfile << "REALLOC_SIZE" << std::dec << " " << size << " REALLOC_PTR 0x" << std::hex << ptr << std::dec << std::endl;
  if (!malloc_outfile.good()) {
    DEBUG_LOG("Error: Failed to write to malloc trace file");
  }
  
  trace_instr_format_t instr = {};
  instr.ip = (unsigned long long int)ip;
  instr.is_malloc = 3; // 3: realloc
  instr.source_memory[0] = size;
  instr.source_memory[1] = ptr;
  malloc_traces.push_back(instr);
}

VOID ReallocAfter(ADDRINT ret)
{
  malloc_outfile << "REALLOC_RET 0x" << std::hex << ret << std::dec << std::endl;
  if (!malloc_outfile.good()) {
    DEBUG_LOG("Error: Failed to write to malloc trace file");
  }
  
  if (!malloc_traces.empty()) {
    curr_instr = malloc_traces.back();
    curr_instr.destination_memory[0] = ret;
    WriteCurrentInstruction(); // Writes to ALL segment files for realloc
    malloc_traces.pop_back();
  } else {
    DEBUG_LOG("Warning: ReallocAfter called without matching ReallocBefore");
  }
}

void ResetCurrentInstruction(VOID* ip)
{
  curr_instr = {};
  curr_instr.ip = (unsigned long long int)ip;
}

BOOL ShouldWrite()
{
  ++instrCount;
  
  // In simpoint mode, check if we should switch segments
  if (useSimpointMode) {
    // Check if we've reached the start of the next segment
    while (currentSegmentIndex < simpointSegments.size() && 
           instrCount >= simpointSegments[currentSegmentIndex]) {
      SwitchToNextSegment();
    }
    
    // Only write if we're currently in an active segment
    return (currentSegmentIndex > 0 && currentSegmentIndex <= simpointSegments.size());
  }
  
  // Original non-simpoint logic
  if (KnobTraceInstructions.Value() == 0) {
    return instrCount > KnobSkipInstructions.Value();
  }
  return (instrCount > KnobSkipInstructions.Value()) && (instrCount <= (KnobTraceInstructions.Value() + KnobSkipInstructions.Value()));
}

void WriteCurrentInstruction()
{
  // ALL instructions (including malloc) go to buffer for current segment
  // This ensures correct ordering within the current segment
  
  // Defensive check: Validate currentSegmentIndex bounds
  if (currentSegmentIndex == 0) {
    DEBUG_LOG("Warning: WriteCurrentInstruction called before any segment is active (currentSegmentIndex=0)");
    return;
  }
  
  if (currentSegmentIndex > segmentFiles.size()) {
    DEBUG_LOG("Error: currentSegmentIndex (" << currentSegmentIndex 
              << ") exceeds segmentFiles size (" << segmentFiles.size() << ")");
    return;
  }
  
  // Defensive check: Validate file pointer
  size_t fileIndex = currentSegmentIndex - 1;
  if (segmentFiles[fileIndex] == nullptr) {
    DEBUG_LOG("Error: segmentFiles[" << fileIndex << "] is null pointer");
    return;
  }
  
  if (!segmentFiles[fileIndex]->is_open()) {
    DEBUG_LOG("Error: segmentFiles[" << fileIndex << "] is not open");
    return;
  }
  
  // Safe to proceed with buffering - ALL instructions use buffer
  try {
    instr_buffer.push_back(curr_instr);
  } catch (const std::exception& e) {
    DEBUG_LOG("Error: Failed to add instruction to buffer: " << e.what());
    return;
  }
  
  // For malloc/calloc/realloc/free instructions, also write to ALL other segment files immediately
  // This ensures each trace file contains complete memory operation history
  if (curr_instr.is_malloc != 0) {
    // Write to all OTHER segment files (not the current one, as it's already in the buffer)
    for (size_t i = 0; i < segmentFiles.size(); i++) {
      if (i == fileIndex) {
        continue; // Skip current segment file (already buffered)
      }
      
      // Defensive check: validate file pointer before access
      if (segmentFiles[i] == nullptr) {
        DEBUG_LOG("Warning: segmentFiles[" << i << "] is null pointer, skipping malloc broadcast");
        continue;
      }
      
      if (segmentFiles[i]->is_open()) {
        try {
          segmentFiles[i]->write(reinterpret_cast<const char*>(&curr_instr), 
                                sizeof(trace_instr_format_t));
          if (!segmentFiles[i]->good()) {
            DEBUG_LOG("Error: Failed to write malloc instruction to segment " << i);
          } else {
            // Flush immediately to ensure data persistence for global events
            segmentFiles[i]->flush();
            if (!segmentFiles[i]->good()) {
              DEBUG_LOG("Error: Failed to flush malloc instruction to segment " << i);
            }
          }
        } catch (const std::exception& e) {
          DEBUG_LOG("Error: Exception writing malloc to segment " << i << ": " << e.what());
        }
      }
    }
  }
  
  // When buffer is full, flush to current segment file only
  if (instr_buffer.size() * sizeof(trace_instr_format_t) >= INSTR_BUFFER_SIZE) {
    try {
      segmentFiles[fileIndex]->write(
          reinterpret_cast<const char*>(&instr_buffer[0]), 
          instr_buffer.size() * sizeof(trace_instr_format_t));
      
      if (!segmentFiles[fileIndex]->good()) {
        DEBUG_LOG("Error: Failed to write instruction buffer to segment " << fileIndex);
      }
      instr_buffer.clear();
    } catch (const std::exception& e) {
      DEBUG_LOG("Error: Exception during buffer flush to segment " 
                << fileIndex << ": " << e.what());
      // Don't clear buffer on error to preserve data
    }
  }
}

void BranchOrNot(UINT32 taken)
{
  curr_instr.is_branch = 1;
  curr_instr.branch_taken = taken;
}

template <typename T>
void WriteToSet(T* begin, T* end, UINT32 r)
{
  auto set_end = std::find(begin, end, 0);
  
  // Check if array is full (no zero terminator found)
  if (set_end == end) {
    DEBUG_LOG("Warning: Register/memory operand array is full, dropping operand " << r);
    return; // Prevent out-of-bounds write
  }
  
  auto found_reg = std::find(begin, set_end, r); // check to see if this register is already in the list
  *found_reg = r;
}

/* ===================================================================== */
// Instrumentation callbacks
/* ===================================================================== */

// RTN instrumentation for malloc/free/calloc/realloc
VOID ImageLoad(IMG img, VOID* v)
{
  RTN rtn = RTN_FindByName(img, "malloc");
  if (!RTN_Valid(rtn))
    rtn = RTN_FindByName(img, "__libc_malloc");
  if (RTN_Valid(rtn)) {
    RTN_Open(rtn);
    RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)MallocBefore, IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_INST_PTR, IARG_END);
    RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)MallocAfter, IARG_FUNCRET_EXITPOINT_VALUE, IARG_END);
    RTN_Close(rtn);
  }

  rtn = RTN_FindByName(img, "free");
  if (!RTN_Valid(rtn))
    rtn = RTN_FindByName(img, "__libc_free");
  if (RTN_Valid(rtn)) {
    RTN_Open(rtn);
    RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)FreeBefore, IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_INST_PTR, IARG_END);
    RTN_Close(rtn);
  }

  rtn = RTN_FindByName(img, "calloc");
  if (!RTN_Valid(rtn))
    rtn = RTN_FindByName(img, "__libc_calloc");
  if (RTN_Valid(rtn)) {
    RTN_Open(rtn);
    RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)CallocBefore, IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_FUNCARG_ENTRYPOINT_VALUE, 1, IARG_INST_PTR, IARG_END);
    RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)CallocAfter, IARG_FUNCRET_EXITPOINT_VALUE, IARG_END);
    RTN_Close(rtn);
  }

  rtn = RTN_FindByName(img, "realloc");
  if (!RTN_Valid(rtn))
    rtn = RTN_FindByName(img, "__libc_realloc");
  if (RTN_Valid(rtn)) {
    RTN_Open(rtn);
    RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)ReallocBefore, IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_FUNCARG_ENTRYPOINT_VALUE, 1, IARG_INST_PTR, IARG_END);
    RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)ReallocAfter, IARG_FUNCRET_EXITPOINT_VALUE, IARG_END);
    RTN_Close(rtn);
  }
}

// Is called for every instruction and instruments reads and writes
VOID Instruction(INS ins, VOID* v)
{
  // begin each instruction with this function
  INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ResetCurrentInstruction, IARG_INST_PTR, IARG_END);

  // instrument branch instructions
  if (INS_IsBranch(ins))
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)BranchOrNot, IARG_BRANCH_TAKEN, IARG_END);

  // instrument register reads
  UINT32 readRegCount = INS_MaxNumRRegs(ins);
  for (UINT32 i = 0; i < readRegCount; i++) {
    UINT32 regNum = INS_RegR(ins, i);
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)WriteToSet<unsigned char>, IARG_PTR, curr_instr.source_registers, IARG_PTR,
                   curr_instr.source_registers + NUM_INSTR_SOURCES, IARG_UINT32, regNum, IARG_END);
  }

  // instrument register writes
  UINT32 writeRegCount = INS_MaxNumWRegs(ins);
  for (UINT32 i = 0; i < writeRegCount; i++) {
    UINT32 regNum = INS_RegW(ins, i);
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)WriteToSet<unsigned char>, IARG_PTR, curr_instr.destination_registers, IARG_PTR,
                   curr_instr.destination_registers + NUM_INSTR_DESTINATIONS, IARG_UINT32, regNum, IARG_END);
  }

  // instrument memory reads and writes
  UINT32 memOperands = INS_MemoryOperandCount(ins);

  // Iterate over each memory operand of the instruction.
  for (UINT32 memOp = 0; memOp < memOperands; memOp++) {
    if (INS_MemoryOperandIsRead(ins, memOp))
      INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)WriteToSet<unsigned long long int>, IARG_PTR, curr_instr.source_memory, IARG_PTR,
                     curr_instr.source_memory + NUM_INSTR_SOURCES, IARG_MEMORYOP_EA, memOp, IARG_END);
    if (INS_MemoryOperandIsWritten(ins, memOp))
      INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)WriteToSet<unsigned long long int>, IARG_PTR, curr_instr.destination_memory, IARG_PTR,
                     curr_instr.destination_memory + NUM_INSTR_DESTINATIONS, IARG_MEMORYOP_EA, memOp, IARG_END);
  }

  // finalize each instruction with this function
  INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)ShouldWrite, IARG_END);
  INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)WriteCurrentInstruction, IARG_END);
}

/*!
 * Print out analysis results.
 * This function is called when the application exits.
 * @param[in]   code            exit code of the application
 * @param[in]   v               value specified by the tool in the
 *                              PIN_AddFiniFunction function call
 */
VOID Fini(INT32 code, VOID* v) {
  DEBUG_LOG("Fini callback called with exit code: " << code);
  
  // Flush any remaining pending malloc traces to ALL segment files
  if (!malloc_traces.empty()) {
    DEBUG_LOG("Warning: Flushing " << malloc_traces.size() 
              << " incomplete malloc traces at program exit");
    // Write incomplete malloc instructions to ALL segment files
    for (auto& pending_instr : malloc_traces) {
      WriteMallocToAllSegments(pending_instr);
    }
    malloc_traces.clear();
  }
  
  // Flush instruction buffer for current segment
  // This ensures all instructions (including malloc) are written in correct order
  if (!instr_buffer.empty() && currentSegmentIndex > 0 && currentSegmentIndex <= segmentFiles.size()) {
    // Defensive check: validate file pointer before writing
    size_t fileIndex = currentSegmentIndex - 1;
    if (segmentFiles[fileIndex] != nullptr && segmentFiles[fileIndex]->is_open()) {
      try {
        segmentFiles[fileIndex]->write(
            reinterpret_cast<const char*>(&instr_buffer[0]), 
            instr_buffer.size() * sizeof(trace_instr_format_t));
        if (!segmentFiles[fileIndex]->good()) {
          DEBUG_LOG("Error: Failed to write final instruction buffer to segment " << fileIndex);
        }
        instr_buffer.clear();
      } catch (const std::exception& e) {
        DEBUG_LOG("Error: Exception during final buffer flush: " << e.what());
      }
    } else {
      DEBUG_LOG("Warning: Cannot flush final buffer, segment file is null or not open");
    }
  }
  
  // Close all segment files
  for (size_t i = 0; i < segmentFiles.size(); i++) {
    // Defensive check: validate pointer before access
    if (segmentFiles[i] != nullptr) {
      if (segmentFiles[i]->is_open()) {
        try {
          segmentFiles[i]->close();
          if (!segmentFiles[i]->good()) {
            DEBUG_LOG("Error: Failed to properly close segment file " << i);
          }
        } catch (const std::exception& e) {
          DEBUG_LOG("Error: Exception closing segment file " << i << ": " << e.what());
        }
      }
      delete segmentFiles[i];
      segmentFiles[i] = nullptr; // Prevent dangling pointer
    }
  }
  segmentFiles.clear();
  
  // Close malloc trace file
  if (malloc_outfile.is_open()) {
    malloc_outfile.close();
    if (!malloc_outfile.good()) {
      DEBUG_LOG("Error: Failed to properly close malloc trace file");
    }
  }
  
  if (useSimpointMode) {
    DEBUG_LOG("Simpoint tracing completed. Generated " 
              << segmentFileNames.size() 
              << " segment trace files.");
  }
  
  DEBUG_LOG("All files closed successfully");
  debug_log.close();
}

/*!
 * The main procedure of the tool.
 * This function is called when the application image is loaded but not yet started.
 * @param[in]   argc            total number of elements in the argv array
 * @param[in]   argv            array of command line arguments,
 *                              including pin -t <toolname> -- ...
 */
int main(int argc, char* argv[])
{
  // Initialize PIN library. Print help message if -h(elp) is specified
  // in the command line or the command line is invalid
  PIN_InitSymbols();
  if (PIN_Init(argc, argv))
    return Usage();

  // Open debug log file first (per PIN tool debugging specification)
  debug_log.open("/tmp/champsim_tracer_debug.log", std::ios_base::out | std::ios_base::app);
  if (!debug_log) {
    std::cerr << "Warning: Cannot open debug log file, using stderr" << std::endl;
  } else {
    // Initialize debug logging flag from command line
    enableDebugLog = KnobEnableDebug.Value();
    if (enableDebugLog) {
      debug_log << "ChampSim tracer started at instruction count: 0 (DEBUG MODE ENABLED)" << std::endl;
    } else {
      debug_log << "ChampSim tracer started at instruction count: 0 (DEBUG MODE DISABLED for performance)" << std::endl;
    }
  }

  // Check if simpoint mode is enabled
  if (!KnobSimpointFile.Value().empty()) {
    useSimpointMode = TRUE;
    
    // Read simpoint file and initialize segments
    if (!ReadSimpointFile(KnobSimpointFile.Value(), KnobOutputFile.Value())) {
      DEBUG_LOG("Error: Failed to initialize simpoint mode");
      
      // Clean up debug log before exit
      if (debug_log.is_open()) {
        debug_log.close();
      }
      exit(1);
    }
    
    // Open ALL segment files at initialization
    if (!OpenAllSegmentFiles()) {
      DEBUG_LOG("Error: Failed to open segment files");
      
      // Clean up debug log before exit
      if (debug_log.is_open()) {
        debug_log.close();
      }
      exit(1);
    }
    
    DEBUG_LOG("Simpoint mode enabled. All " << segmentFiles.size() 
              << " segment files opened.");
  } else {
    // Original non-simpoint mode: open single output file
    std::ofstream* singleFile = new std::ofstream();
    singleFile->open(KnobOutputFile.Value().c_str(), std::ios_base::binary | std::ios_base::trunc);
    if (!singleFile->is_open()) {
      DEBUG_LOG("Couldn't open output trace file. Exiting.");
      
      // Clean up debug log before exit
      if (debug_log.is_open()) {
        debug_log.close();
      }
      
      delete singleFile;
      exit(1);
    }
    segmentFiles.push_back(singleFile);
    currentSegmentIndex = 1; // Mark as active (1-based indexing)
    DEBUG_LOG("Opened single output file: " << KnobOutputFile.Value());
  }

  // Pre-allocate instruction buffer capacity to avoid frequent reallocation
  instr_buffer.reserve(INSTR_BUFFER_SIZE / sizeof(trace_instr_format_t));

  // Open malloc trace file
  malloc_outfile.open(KnobMallocOutputFile.Value().c_str(), std::ios_base::out);
  if (!malloc_outfile) {
    DEBUG_LOG("Couldn't open malloc trace file. Exiting.");
    
    // Clean up all opened resources before exit
    for (size_t i = 0; i < segmentFiles.size(); i++) {
      if (segmentFiles[i] != nullptr) {
        if (segmentFiles[i]->is_open()) {
          segmentFiles[i]->close();
        }
        delete segmentFiles[i];
        segmentFiles[i] = nullptr;
      }
    }
    segmentFiles.clear();
    
    // Clean up debug log before exit
    if (debug_log.is_open()) {
      debug_log.close();
    }
    
    exit(1);
  }
  DEBUG_LOG("Opened malloc trace file: " << KnobMallocOutputFile.Value());

  // Register function to be called to instrument instructions
  INS_AddInstrumentFunction(Instruction, 0);

  // Register image load callback to instrument malloc/free/calloc/realloc
  IMG_AddInstrumentFunction(ImageLoad, 0);

  // Register function to be called when the application exits
  PIN_AddFiniFunction(Fini, 0);

  DEBUG_LOG("All instrumentation registered, starting program");

  // Start the program, never returns
  PIN_StartProgram();

  return 0;
}
