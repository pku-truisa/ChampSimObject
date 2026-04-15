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
    // Remove the object from the map
    active_objects.erase(it);
  }
}

} // namespace champsim