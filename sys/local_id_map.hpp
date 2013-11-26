#pragma once

#include <boost/any.hpp>
#include <tbb/concurrent_hash_map.h>

#include <sys/trans.hpp>

namespace ebbrt {
class LocalIdMap {
  typedef tbb::concurrent_hash_map<
      EbbId, boost::any, tbb::tbb_hash_compare<EbbId>,
      std::allocator<std::pair<const EbbId, boost::any> > > map_type;
  map_type map_;

public:
  static void Init();
  static LocalIdMap &HandleFault(EbbId id);

  typedef map_type::const_accessor const_accessor;
  typedef map_type::accessor accessor;
  typedef map_type::value_type value_type;
  bool find(const_accessor& result, const EbbId& key) const;
  bool find(accessor& result, const EbbId& key);
  bool insert(const_accessor& result, const EbbId& key);
  bool insert(accessor& result, const EbbId& key);
  bool insert(const_accessor& result, const value_type& value);
  bool insert(accessor& result, const value_type& value);
  bool insert(const value_type& value);
  bool erase(const EbbId& key);
  bool erase(const_accessor& item_accessor);
  bool erase(accessor& item_accessor);
};

constexpr auto local_id_map = EbbRef<LocalIdMap>{ local_id_map_id };
}
