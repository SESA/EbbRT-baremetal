#include <boost/container/flat_map.hpp>

#include <sys/cpu.hpp>
#include <sys/event_manager.hpp>
#include <sys/local_id_map.hpp>
#include <sys/page_allocator.hpp>
#include <sys/vmem.hpp>

using namespace ebbrt;

typedef boost::container::flat_map<size_t, EventManager *> rep_map_t;

void EventManager::Init() {
  local_id_map->insert(std::make_pair(event_manager_id, rep_map_t()));
}

EventManager &EventManager::HandleFault(EbbId id) {
  kassert(id == event_manager_id);
  {
    // Acquire read only to find rep
    LocalIdMap::const_accessor accessor;
    auto found = local_id_map->find(accessor, id);
    kassert(found);
    auto rep_map = boost::any_cast<rep_map_t>(accessor->second);
    auto it = rep_map.find(my_cpu());
    if (it != rep_map.end()) {
      return *it->second;
    }
  }
  // OK we must construct a rep
  auto rep = new EventManager;
  LocalIdMap::accessor accessor;
  auto found = local_id_map->find(accessor, id);
  kassert(found);
  auto rep_map = boost::any_cast<rep_map_t>(accessor->second);
  rep_map[my_cpu()] = rep;
  cache_ref(id, *rep);
  return *rep;
}

namespace {
const constexpr size_t STACK_NPAGES = 2048; // 8 MB stack
}

class event_stack_fault_handler {
  size_t faulted_pages_;

public:
  event_stack_fault_handler() : faulted_pages_{ 0 } {}
  ~event_stack_fault_handler() {
    kbugon(faulted_pages_ > 0, "Free stack pages!\n");
  }
  void handle_fault(exception_frame *ef, uintptr_t faulted_address) {
    kbugon(faulted_pages_ + 1 >= STACK_NPAGES, "Stack overflow!\n");
    auto page = page_allocator->Alloc();
    kbugon(page == 0);
    map_memory(pfn_down(faulted_address), page);
    faulted_pages_++;
  }
};

EventManager::EventContext::EventContext()
    : stack_{ vmem_allocator->Alloc(STACK_NPAGES,
                                    event_stack_fault_handler()) } {}

uintptr_t EventManager::EventContext::top_of_stack() const {
  return pfn_to_addr(stack_ + STACK_NPAGES);
}

extern "C" __attribute__((noreturn)) void
switch_stack(uintptr_t first_param, uintptr_t stack, void (*func)(uintptr_t));

void EventManager::StartLoop() {
  auto stack_top = active_context_.top_of_stack();
  switch_stack(reinterpret_cast<uintptr_t>(this), stack_top, CallLoop);
}

void EventManager::CallLoop(uintptr_t mgr) {
  auto pmgr = reinterpret_cast<EventManager *>(mgr);
  pmgr->Loop();
}

void EventManager::Loop() {
  while (1) {
    kbugon(tasks_.empty());
    auto f = std::move(tasks_.top());
    tasks_.pop();
    f();
  }
}

void EventManager::SpawnLocal(std::function<void()> func) {
  tasks_.emplace(std::move(func));
}
