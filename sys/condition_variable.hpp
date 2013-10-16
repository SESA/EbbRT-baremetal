#pragma once

#include <boost/intrusive/slist.hpp>
#include <sys/semaphore.hpp>
#include <sys/spinlock.hpp>

namespace ebbrt {
class condition_variable {
  struct cv_node {
    semaphore sem;
    boost::intrusive::slist_member_hook<> member_hook;
    cv_node() : sem{0} {}
  };
  boost::intrusive::slist<cv_node,
                          boost::intrusive::member_hook<
                              cv_node, boost::intrusive::slist_member_hook<>,
                              &cv_node::member_hook> > list_;
  spinlock lock_;

public:
  void broadcast() {
    std::lock_guard<spinlock> lock{ lock_ };
    for (auto &node : list_) {
      node.sem.signal();
    }
  }
  void wait(spinlock &other) {
    cv_node node;
    {
      std::lock_guard<spinlock> lock{ lock_ };
      list_.push_front(node);
      other.unlock();
    }
    node.sem.wait();
    other.lock();
  }
};
}
