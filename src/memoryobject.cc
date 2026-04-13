#include "memoryobject.h"

#include <vector>
#include <map>

namespace champsim
{
// Global ID counter
uint64_t next_memory_object_id = 0;

// Global data structures definitions
std::vector<MemoryObject> all_objects;
std::map<champsim::address, ActiveObject*> address_to_object;

// Function to add a memory object
void add_memory_object(MemoryObject obj) {
  all_objects.push_back(std::move(obj));
  
  // Create an ActiveObject using the constructor
  auto* active_obj = new ActiveObject(
      all_objects.back().object_id,
      all_objects.back().start_address,
      all_objects.back().size
  );
  
  address_to_object[all_objects.back().start_address] = active_obj;
}

// Function to find a memory object by address
ActiveObject* find_memory_object(champsim::address addr) {
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

// Function to delete a memory object by address
void delete_memory_object(champsim::address addr) {
  auto upper = address_to_object.upper_bound(addr);
  if (upper == address_to_object.begin()) {
    return; // No object found
  }

  auto it = std::prev(upper);
  if (it->second && it->second->contains_address(addr)) {
    // Delete only the ActiveObject and remove it from the map
    delete it->second;
    address_to_object.erase(it);
  }
}

} // namespace champsim