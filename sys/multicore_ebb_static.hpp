#pragma once

#include <boost/container/flat_map.hpp>

#include <sys/cpu.hpp>
#include <sys/local_id_map.hpp>

namespace ebbrt {
template <typename T> class multicore_ebb_static {
  typedef boost::container::flat_map<size_t, T*> rep_map_t;
 public:
  static void Init() {
    local_id_map->insert(std::make_pair(T::static_id, rep_map_t()));
  }
  static T& HandleFault(EbbId id) {
    kassert(id == T::static_id);
    {
      // acquire read only to find rep
      LocalIdMap::const_accessor accessor;
      auto found = local_id_map->find(accessor, id);
      kassert(found);
      auto rep_map = boost::any_cast<rep_map_t>(accessor->second);
      auto it = rep_map.find(my_cpu());
      if (it != rep_map.end()) {
        cache_ref(id, *it->second);
        return *it->second;
      }
    }
    // we failed to find a rep, we must construct one
    auto rep = new T;
    //TODO: make the rep_map thread safe so we can acquire r/o access
    LocalIdMap::accessor accessor;
    auto found = local_id_map->find(accessor, id);
    kassert(found);
    auto rep_map = boost::any_cast<rep_map_t>(accessor->second);
    rep_map[my_cpu()] = rep;
    cache_ref(id, *rep);
    return *rep;
  }
};
}
