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

#include "memoryobject_printer.h"
#include "memoryobject.h"

#include <fstream>
#include <fmt/core.h>

namespace champsim
{
void memoryobject_printer::print(const std::vector<MemoryObject>& objects, std::string_view filename)
{
  print_to_file(objects, filename);
}

void memoryobject_printer::print(const std::vector<MemoryObject>& objects)
{
  print_to_file(objects, m_filename);
}

void memoryobject_printer::print_to_file(const std::vector<MemoryObject>& objects, std::string_view filename)
{
  std::ofstream outfile(filename);
  if (!outfile.is_open()) {
    fmt::print(stderr, "Error: Could not open file {} for writing\n", filename);
    return;
  }

  // Print header
  outfile << "# Memory Object Statistics\n";
  outfile << "# Format: object_id, start_address, size, total_accesses, total_miss_latency_cycles, ";
  outfile << "accesses[LOAD], accesses[RFO], accesses[PREFETCH], accesses[WRITE], accesses[TRANSLATION], ";
  outfile << "hits[LOAD], hits[RFO], hits[PREFETCH], hits[WRITE], hits[TRANSLATION], ";
  outfile << "misses[LOAD], misses[RFO], misses[PREFETCH], misses[WRITE], misses[TRANSLATION], ";
  outfile << "pf_requested, pf_issued, pf_useful, pf_useless, pf_fill, mshr_merge, mshr_return, first_access_ip\n";

  // Print statistics for each object
  for (const auto& obj : objects) {
    outfile << obj.object_id << ", " << obj.start_address.to<uint64_t>() << ", " << obj.size << ", ";
    outfile << obj.total_accesses << ", " << obj.total_miss_latency_cycles << ", ";

    // Print accesses by type
    outfile << obj.accesses[static_cast<size_t>(access_type::LOAD)] << ", ";
    outfile << obj.accesses[static_cast<size_t>(access_type::RFO)] << ", ";
    outfile << obj.accesses[static_cast<size_t>(access_type::PREFETCH)] << ", ";
    outfile << obj.accesses[static_cast<size_t>(access_type::WRITE)] << ", ";
    outfile << obj.accesses[static_cast<size_t>(access_type::TRANSLATION)] << ", ";

    // Print hits by type
    outfile << obj.hits[static_cast<size_t>(access_type::LOAD)] << ", ";
    outfile << obj.hits[static_cast<size_t>(access_type::RFO)] << ", ";
    outfile << obj.hits[static_cast<size_t>(access_type::PREFETCH)] << ", ";
    outfile << obj.hits[static_cast<size_t>(access_type::WRITE)] << ", ";
    outfile << obj.hits[static_cast<size_t>(access_type::TRANSLATION)] << ", ";

    // Print misses by type
    outfile << obj.misses[static_cast<size_t>(access_type::LOAD)] << ", ";
    outfile << obj.misses[static_cast<size_t>(access_type::RFO)] << ", ";
    outfile << obj.misses[static_cast<size_t>(access_type::PREFETCH)] << ", ";
    outfile << obj.misses[static_cast<size_t>(access_type::WRITE)] << ", ";
    outfile << obj.misses[static_cast<size_t>(access_type::TRANSLATION)] << ", ";

    // Print prefetch statistics
    outfile << obj.pf_requested << ", " << obj.pf_issued << ", ";
    outfile << obj.pf_useful << ", " << obj.pf_useless << ", " << obj.pf_fill << ", ";

    // Print MSHR statistics
    outfile << obj.mshr_merge << ", " << obj.mshr_return << ", ";

    // Print first access IP
    outfile << obj.first_access_ip.to<uint64_t>() << "\n";

    // Print per-cache-level statistics
    for (const auto& [cache_name, cache_stats] : obj.get_cache_stats_by_level()) {
      outfile << "# Cache Level: " << cache_name << "\n";
      outfile << "# Object ID: " << obj.object_id << "\n";
      outfile << "total_accesses, total_miss_latency_cycles, ";
      outfile << "accesses[LOAD], accesses[RFO], accesses[PREFETCH], accesses[WRITE], accesses[TRANSLATION], ";
      outfile << "hits[LOAD], hits[RFO], hits[PREFETCH], hits[WRITE], hits[TRANSLATION], ";
      outfile << "misses[LOAD], misses[RFO], misses[PREFETCH], misses[WRITE], misses[TRANSLATION], ";
      outfile << "pf_requested, pf_issued, pf_useful, pf_useless, pf_fill, mshr_merge, mshr_return\n";
      outfile << cache_stats.total_accesses << ", " << cache_stats.total_miss_latency_cycles << ", ";

      // Print accesses by type
      outfile << cache_stats.accesses[static_cast<size_t>(access_type::LOAD)] << ", ";
      outfile << cache_stats.accesses[static_cast<size_t>(access_type::RFO)] << ", ";
      outfile << cache_stats.accesses[static_cast<size_t>(access_type::PREFETCH)] << ", ";
      outfile << cache_stats.accesses[static_cast<size_t>(access_type::WRITE)] << ", ";
      outfile << cache_stats.accesses[static_cast<size_t>(access_type::TRANSLATION)] << ", ";

      // Print hits by type
      outfile << cache_stats.hits[static_cast<size_t>(access_type::LOAD)] << ", ";
      outfile << cache_stats.hits[static_cast<size_t>(access_type::RFO)] << ", ";
      outfile << cache_stats.hits[static_cast<size_t>(access_type::PREFETCH)] << ", ";
      outfile << cache_stats.hits[static_cast<size_t>(access_type::WRITE)] << ", ";
      outfile << cache_stats.hits[static_cast<size_t>(access_type::TRANSLATION)] << ", ";

      // Print misses by type
      outfile << cache_stats.misses[static_cast<size_t>(access_type::LOAD)] << ", ";
      outfile << cache_stats.misses[static_cast<size_t>(access_type::RFO)] << ", ";
      outfile << cache_stats.misses[static_cast<size_t>(access_type::PREFETCH)] << ", ";
      outfile << cache_stats.misses[static_cast<size_t>(access_type::WRITE)] << ", ";
      outfile << cache_stats.misses[static_cast<size_t>(access_type::TRANSLATION)] << ", ";

      // Print prefetch statistics
      outfile << cache_stats.pf_requested << ", " << cache_stats.pf_issued << ", ";
      outfile << cache_stats.pf_useful << ", " << cache_stats.pf_useless << ", " << cache_stats.pf_fill << ", ";

      // Print MSHR statistics
      outfile << cache_stats.mshr_merge << ", " << cache_stats.mshr_return << "\n";
    }
  }

  outfile.close();
  fmt::print("Memory object statistics written to {}\n", filename);
}
} // namespace champsim
