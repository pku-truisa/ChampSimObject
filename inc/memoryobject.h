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

#ifndef MEMORYOBJECT_H
#define MEMORYOBJECT_H

#include <array>
#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>

#include "access_type.h"
#include "address.h"
#include "chrono.h"

namespace champsim
{
struct MemoryObject {
  using id_type = uint64_t;
  id_type object_id = 0;

  champsim::address start_address{};
  std::size_t size = 0;
  champsim::chrono::clock::time_point allocation_time{};

  // Total memory accesses associated with this object.
  uint64_t total_accesses = 0;
  uint64_t total_miss_latency_cycles = 0;

  std::array<uint64_t, static_cast<std::size_t>(access_type::NUM_TYPES)> accesses = {};
  std::array<uint64_t, static_cast<std::size_t>(access_type::NUM_TYPES)> hits = {};
  std::array<uint64_t, static_cast<std::size_t>(access_type::NUM_TYPES)> misses = {};

  uint64_t pf_requested = 0;
  uint64_t pf_issued = 0;
  uint64_t pf_useful = 0;
  uint64_t pf_useless = 0;
  uint64_t pf_fill = 0;

  // Per-cache-level statistics keyed by cache instance name.
  struct CacheLevelStats {
    uint64_t total_accesses = 0;
    uint64_t total_miss_latency_cycles = 0;
    std::array<uint64_t, static_cast<std::size_t>(access_type::NUM_TYPES)> accesses = {};
    std::array<uint64_t, static_cast<std::size_t>(access_type::NUM_TYPES)> hits = {};
    std::array<uint64_t, static_cast<std::size_t>(access_type::NUM_TYPES)> misses = {};
    uint64_t pf_requested = 0;
    uint64_t pf_issued = 0;
    uint64_t pf_useful = 0;
    uint64_t pf_useless = 0;
    uint64_t pf_fill = 0;
  };

  std::unordered_map<std::string, CacheLevelStats> cache_stats_by_level;

  // Constructor
  MemoryObject(id_type id, champsim::address addr, std::size_t sz, champsim::chrono::clock::time_point time)
      : object_id(id), start_address(addr), size(sz), allocation_time(time) {}

  // Check if an address belongs to this object.
  [[nodiscard]] bool contains_address(champsim::address addr) const {
    return addr >= start_address && addr < (start_address + champsim::address{size});
  }

  // Update object-level access statistics.
  void record_access(access_type type, bool hit, uint64_t latency = 0) {
    const auto index = static_cast<std::size_t>(type);
    ++total_accesses;
    ++accesses[index];
    if (hit) {
      ++hits[index];
    } else {
      ++misses[index];
      total_miss_latency_cycles += latency;
    }
  }

  CacheLevelStats& level_stats(const std::string& cache_name) {
    return cache_stats_by_level.try_emplace(cache_name).first->second;
  }

  const CacheLevelStats& level_stats(const std::string& cache_name) const {
    static const CacheLevelStats empty_stats;
    auto it = cache_stats_by_level.find(cache_name);
    return (it != cache_stats_by_level.end()) ? it->second : empty_stats;
  }

  void record_cache_level_access(const std::string& cache_name, access_type type, bool hit, uint64_t latency = 0) {
    auto& stats = level_stats(cache_name);
    const auto index = static_cast<std::size_t>(type);
    ++stats.total_accesses;
    ++stats.accesses[index];
    if (hit) {
      ++stats.hits[index];
    } else {
      ++stats.misses[index];
      stats.total_miss_latency_cycles += latency;
    }
  }

  void record_cache_level_prefetch_request(const std::string& cache_name) { ++level_stats(cache_name).pf_requested; }
  void record_cache_level_prefetch_issue(const std::string& cache_name) { ++level_stats(cache_name).pf_issued; }
  void record_cache_level_prefetch_useful(const std::string& cache_name) { ++level_stats(cache_name).pf_useful; }
  void record_cache_level_prefetch_useless(const std::string& cache_name) { ++level_stats(cache_name).pf_useless; }
  void record_cache_level_prefetch_fill(const std::string& cache_name) { ++level_stats(cache_name).pf_fill; }

  void record_prefetch_request() { ++pf_requested; }
  void record_prefetch_issue() { ++pf_issued; }
  void record_prefetch_useful() { ++pf_useful; }
  void record_prefetch_useless() { ++pf_useless; }
  void record_prefetch_fill() { ++pf_fill; }

  // Get object statistics
  uint64_t get_total_accesses() const { return total_accesses; }
  uint64_t get_total_miss_latency_cycles() const { return total_miss_latency_cycles; }
  const std::array<uint64_t, static_cast<std::size_t>(access_type::NUM_TYPES)>& get_accesses() const { return accesses; }
  const std::array<uint64_t, static_cast<std::size_t>(access_type::NUM_TYPES)>& get_hits() const { return hits; }
  const std::array<uint64_t, static_cast<std::size_t>(access_type::NUM_TYPES)>& get_misses() const { return misses; }
  uint64_t get_pf_requested() const { return pf_requested; }
  uint64_t get_pf_issued() const { return pf_issued; }
  uint64_t get_pf_useful() const { return pf_useful; }
  uint64_t get_pf_useless() const { return pf_useless; }
  uint64_t get_pf_fill() const { return pf_fill; }
  const std::unordered_map<std::string, CacheLevelStats>& get_cache_stats_by_level() const { return cache_stats_by_level; }
};

struct ActiveObject {
  using id_type = uint64_t;
  id_type object_id = 0;

  champsim::address start_address{};
  std::size_t size = 0;

  // Default constructor
  ActiveObject() = default;

  // Constructor with parameters
  ActiveObject(id_type id, champsim::address addr, std::size_t sz)
      : object_id(id), start_address(addr), size(sz) {}

  // Get the end address of this object
  [[nodiscard]] champsim::address end_address() const {
    return start_address + champsim::address{size};
  }

  // Check if an address belongs to this object
  [[nodiscard]] bool contains_address(champsim::address addr) const {
    return addr >= start_address && addr < end_address();
  }
};

// Global data structures
extern std::vector<MemoryObject> all_objects;
extern std::map<champsim::address, ActiveObject*> address_to_object;

// Global ID counter for memory objects
extern uint64_t next_memory_object_id;

// Functions to manage objects
void add_memory_object(MemoryObject obj);
ActiveObject* find_memory_object(champsim::address addr);
}

#endif