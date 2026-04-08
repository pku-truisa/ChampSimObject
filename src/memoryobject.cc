#include "memoryobject.h"

#include <deque>
#include <map>

namespace champsim
{
// Global ID counter
uint64_t next_memory_object_id = 0;

// Global data structures definitions
std::deque<MemoryObject> all_objects;
std::map<champsim::address, MemoryObject*> address_to_object;

// Function to add a memory object
void add_memory_object(MemoryObject obj) {
  all_objects.push_back(std::move(obj));
  address_to_object[all_objects.back().start_address] = &all_objects.back();
}

// Function to find a memory object by address
MemoryObject* find_memory_object(champsim::address addr) {
  auto upper = address_to_object.upper_bound(addr);
  if (upper == address_to_object.begin()) {
    return nullptr;
  }

  auto it = std::prev(upper);
  if (it->second && it->second->contains_address(addr)) {
    return it->second;
  }

  return nullptr;
}

} // namespace champsim