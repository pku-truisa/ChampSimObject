#include "memoryobject.h"

#include <vector>
#include <map>

namespace champsim
{
// Global ID counter
uint64_t next_memory_object_id = 0;

// Global data structures definitions
std::vector<MemoryObject> all_objects;
std::map<champsim::address, MemoryObject*> active_objects;

// Function to add a memory object
void add_memory_object(MemoryObject obj) {
  all_objects.push_back(std::move(obj));
  
  // Use the MemoryObject pointer directly
  
  active_objects[all_objects.back().start_address] = &all_objects.back();
}

// Function to find a memory object by address
MemoryObject* find_memory_object(champsim::address addr) {
  auto upper = active_objects.upper_bound(addr);
  if (upper == active_objects.begin()) {
    return nullptr;
  }

  auto it = std::prev(upper);
  if (it->second && it->second->contains_address(addr)) {
    return it->second;
  }

  return nullptr;
}

// Function to delete a memory object by address
void delete_memory_object(champsim::address addr) {
  auto upper = active_objects.upper_bound(addr);
  if (upper == active_objects.begin()) {
    return; // No object found
  }

  auto it = std::prev(upper);
  if (it->second && it->second->contains_address(addr)) {
    // Get pointer to the object being deleted
    MemoryObject* obj_ptr = it->second;

    // Find the corresponding object in all_objects and update its statistics
    for (auto& obj : all_objects) {
      if (obj.object_id == obj_ptr->object_id) {
        // Update total statistics
        obj.total_accesses += obj_ptr->total_accesses;
        obj.total_miss_latency_cycles += obj_ptr->total_miss_latency_cycles;

        // Update per-access-type statistics
        for (size_t i = 0; i < static_cast<size_t>(access_type::NUM_TYPES); ++i) {
          obj.accesses[i] += obj_ptr->accesses[i];
          obj.hits[i] += obj_ptr->hits[i];
          obj.misses[i] += obj_ptr->misses[i];
        }

        // Update prefetch statistics
        obj.pf_requested += obj_ptr->pf_requested;
        obj.pf_issued += obj_ptr->pf_issued;
        obj.pf_useful += obj_ptr->pf_useful;
        obj.pf_useless += obj_ptr->pf_useless;
        obj.pf_fill += obj_ptr->pf_fill;

        // Update MSHR statistics
        obj.mshr_merge += obj_ptr->mshr_merge;
        obj.mshr_return += obj_ptr->mshr_return;

        // Update per-cache-level statistics
        for (const auto& [cache_name, cache_stats] : obj_ptr->cache_stats_by_level) {
          auto& target_stats = obj.cache_stats_by_level[cache_name];
          target_stats.total_accesses += cache_stats.total_accesses;
          target_stats.total_miss_latency_cycles += cache_stats.total_miss_latency_cycles;

          for (size_t i = 0; i < static_cast<size_t>(access_type::NUM_TYPES); ++i) {
            target_stats.accesses[i] += cache_stats.accesses[i];
            target_stats.hits[i] += cache_stats.hits[i];
            target_stats.misses[i] += cache_stats.misses[i];
          }

          target_stats.pf_requested += cache_stats.pf_requested;
          target_stats.pf_issued += cache_stats.pf_issued;
          target_stats.pf_useful += cache_stats.pf_useful;
          target_stats.pf_useless += cache_stats.pf_useless;
          target_stats.pf_fill += cache_stats.pf_fill;
          target_stats.mshr_merge += cache_stats.mshr_merge;
          target_stats.mshr_return += cache_stats.mshr_return;
        }

        // Update first access IP if not already set
        if (obj.first_access_ip == champsim::address{} && obj_ptr->first_access_ip != champsim::address{}) {
          obj.first_access_ip = obj_ptr->first_access_ip;
        }

        break; // Found the matching object, no need to continue
      }
    }

    // Remove the object from the map
    active_objects.erase(it);
  }
}

// Function to flush statistics from all active objects to their corresponding entries in all_objects
void flush_active_objects_stats() {
  // Iterate through all active objects
  for (const auto& [addr, obj_ptr] : active_objects) {
    // Find the corresponding object in all_objects
    for (auto& obj : all_objects) {
      if (obj.object_id == obj_ptr->object_id) {
        // Update total statistics
        obj.total_accesses += obj_ptr->total_accesses;
        obj.total_miss_latency_cycles += obj_ptr->total_miss_latency_cycles;

        // Update per-access-type statistics
        for (size_t i = 0; i < static_cast<size_t>(access_type::NUM_TYPES); ++i) {
          obj.accesses[i] += obj_ptr->accesses[i];
          obj.hits[i] += obj_ptr->hits[i];
          obj.misses[i] += obj_ptr->misses[i];
        }

        // Update prefetch statistics
        obj.pf_requested += obj_ptr->pf_requested;
        obj.pf_issued += obj_ptr->pf_issued;
        obj.pf_useful += obj_ptr->pf_useful;
        obj.pf_useless += obj_ptr->pf_useless;
        obj.pf_fill += obj_ptr->pf_fill;

        // Update MSHR statistics
        obj.mshr_merge += obj_ptr->mshr_merge;
        obj.mshr_return += obj_ptr->mshr_return;

        // Update per-cache-level statistics
        for (const auto& [cache_name, cache_stats] : obj_ptr->cache_stats_by_level) {
          auto& target_stats = obj.cache_stats_by_level[cache_name];
          target_stats.total_accesses += cache_stats.total_accesses;
          target_stats.total_miss_latency_cycles += cache_stats.total_miss_latency_cycles;

          for (size_t i = 0; i < static_cast<size_t>(access_type::NUM_TYPES); ++i) {
            target_stats.accesses[i] += cache_stats.accesses[i];
            target_stats.hits[i] += cache_stats.hits[i];
            target_stats.misses[i] += cache_stats.misses[i];
          }

          target_stats.pf_requested += cache_stats.pf_requested;
          target_stats.pf_issued += cache_stats.pf_issued;
          target_stats.pf_useful += cache_stats.pf_useful;
          target_stats.pf_useless += cache_stats.pf_useless;
          target_stats.pf_fill += cache_stats.pf_fill;
          target_stats.mshr_merge += cache_stats.mshr_merge;
          target_stats.mshr_return += cache_stats.mshr_return;
        }

        // Update first access IP if not already set
        if (obj.first_access_ip == champsim::address{} && obj_ptr->first_access_ip != champsim::address{}) {
          obj.first_access_ip = obj_ptr->first_access_ip;
        }

        break; // Found matching object, no need to continue
      }
    }
  }
}

} // namespace champsim