#include <unordered_map>

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

class event_stack_fault_handler : public VMemAllocator::page_fault_handler_t {
  std::unordered_map<pfn_t, pfn_t> mappings_;

public:
  event_stack_fault_handler() = default;
  event_stack_fault_handler(const event_stack_fault_handler &) = delete;
  event_stack_fault_handler &operator=(const event_stack_fault_handler &) =
      delete;
  ~event_stack_fault_handler() {
    kbugon(!mappings_.empty(), "Free stack pages!\n");
  }
  void handle_fault(exception_frame *ef, uintptr_t faulted_address) override {
    auto page = pfn_down(faulted_address);
    auto it = mappings_.find(page);
    if (it == mappings_.end()) {
      kbugon(mappings_.size() + 1 >= STACK_NPAGES, "Stack overflow!\n");
      auto backing_page = page_allocator->Alloc();
      kbugon(backing_page == 0, "Failed to allocate page for stack\n");
      map_memory(page, backing_page);
      mappings_[page] = backing_page;
    } else {
      map_memory(page, it->second);
    }
  }
};

EventManager::EventContext::EventContext() {
  auto fault_handler = new event_stack_fault_handler;
  stack_ = vmem_allocator->Alloc(
      STACK_NPAGES, std::unique_ptr<event_stack_fault_handler>(fault_handler));
}

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
    if (!tasks_.empty()) {
      auto f = std::move(tasks_.top());
      tasks_.pop();
      try {
        f();
      }
      catch (std::exception &e) {
        kabort("Unhandled exception caught: %s\n", e.what());
      }
      catch (...) {
        kabort("Unhandled exception caught!\n");
      }
    } else {
      asm volatile("hlt");
    }
  }
}

void EventManager::SpawnLocal(std::function<void()> func) {
  tasks_.emplace(std::move(func));
}
