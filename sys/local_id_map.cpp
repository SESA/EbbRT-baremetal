#include <sys/explicitly_constructed.hpp>
#include <sys/local_id_map.hpp>

using namespace ebbrt;

namespace {
explicitly_constructed<LocalIdMap> the_map;
}

void LocalIdMap::Init() { the_map.construct(); }

LocalIdMap &LocalIdMap::HandleFault(EbbId id) {
  kassert(id == local_id_map_id);
  auto &ref = *the_map;
  cache_ref(id, ref);
  return ref;
}

bool LocalIdMap::insert(const value_type &value) { return map_.insert(value); }

bool LocalIdMap::find(const_accessor &result, const EbbId &key) const {
  return map_.find(result, key);
}
