#pragma once

#include <boost/intrusive/slist.hpp>

namespace ebbrt {
struct free_object {
  boost::intrusive::slist_member_hook<> member_hook;

  void *addr() { return this; }
};

typedef boost::intrusive::slist<
    free_object, boost::intrusive::cache_last<true>,
    boost::intrusive::member_hook<free_object,
                                  boost::intrusive::slist_member_hook<>,
                                  &free_object::member_hook> > free_object_list;

typedef boost::intrusive::slist<
    free_object, boost::intrusive::constant_time_size<false>,
    boost::intrusive::member_hook<
        free_object, boost::intrusive::slist_member_hook<>,
        &free_object::member_hook> > compact_free_object_list;
}
