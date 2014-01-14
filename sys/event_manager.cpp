#include <unordered_map>

#include <boost/container/flat_map.hpp>

#include <sys/cpu.hpp>
#include <sys/event_manager.hpp>
#include <sys/local_id_map.hpp>
#include <sys/page_allocator.hpp>
#include <sys/vmem.hpp>

using namespace ebbrt;

typedef boost::container::flat_map<size_t, EventManager*> rep_map_t;

void EventManager::Init() {
  local_id_map->insert(std::make_pair(event_manager_id, rep_map_t()));
}

EventManager& EventManager::HandleFault(EbbId id) {
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
const constexpr size_t STACK_NPAGES = 2048;  // 8 MB stack
}

class event_stack_fault_handler : public VMemAllocator::page_fault_handler_t {
  std::unordered_map<pfn_t, pfn_t> mappings_;

 public:
  event_stack_fault_handler() = default;
  event_stack_fault_handler(const event_stack_fault_handler&) = delete;
  event_stack_fault_handler& operator=(const event_stack_fault_handler&) =
      delete;
  ~event_stack_fault_handler() {
    kbugon(!mappings_.empty(), "Free stack pages!\n");
  }
  void handle_fault(exception_frame* ef, uintptr_t faulted_address) override {
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

extern "C" __attribute__((noreturn)) void switch_stack(uintptr_t first_param,
                                                       uintptr_t stack,
                                                       void (*func)(uintptr_t));

void EventManager::StartProcessingEvents() {
  auto stack_top = pfn_to_addr(stack_ + STACK_NPAGES);
  my_cpu().set_event_stack(stack_top);
  switch_stack(reinterpret_cast<uintptr_t>(this), stack_top, CallProcess);
}

void EventManager::CallProcess(uintptr_t mgr) {
  auto pmgr = reinterpret_cast<EventManager*>(mgr);
  pmgr->Process();
}

namespace {
void invoke_function(std::function<void()>& f) {
  try {
    f();
  }
  catch (std::exception & e) {
    kabort("Unhandled exception caught: %s\n", e.what());
  }
  catch (...) {
    kabort("Unhandled exception caught!\n");
  }
}
}

void EventManager::Process() {
//process an interrupt without halting
//the sti instruction starts processing interrupts *after* the next
//instruction is executed (to allow for a halt for example). The nop gives us
//a one instruction window to process an interrupt (before the cli)
process:
  asm volatile("sti;"
               "nop;"
               "cli;");
  //If an interrupt was processed then we would not reach this code (the
  //interrupt does not return here but instead to the top of this function)

  if (!tasks_.empty()) {
    auto f = std::move(tasks_.top());
    tasks_.pop();
    invoke_function(f);
    //if we had a task to execute, then we go to the top again
    goto process;
  }

  //We only reach here if we had no interrupts to process or tasks to run. We
  //halt until an interrupt wakes us
  asm volatile("sti;"
               "hlt;");
  kabort("Woke up from halt?!?!");
}

namespace {
pfn_t allocate_stack() {
  auto fault_handler = new event_stack_fault_handler;
  return vmem_allocator->Alloc(
      STACK_NPAGES, std::unique_ptr<event_stack_fault_handler>(fault_handler));
}
}

EventManager::EventManager() : vector_idx_(32) { stack_ = allocate_stack(); }

void EventManager::SpawnLocal(std::function<void()> func) {
  tasks_.emplace(std::move(func));
}

extern "C" void save_context_and_switch(uintptr_t first_param,
                                        uintptr_t stack,
                                        void (*func)(uintptr_t),
                                        EventManager::EventContext& context);

void EventManager::SaveContext(EventContext& context) {
  context.stack = stack_;
  stack_ = allocate_stack();
  auto stack_top = pfn_to_addr(stack_ + STACK_NPAGES);
  my_cpu().set_event_stack(stack_top);
  save_context_and_switch(
      reinterpret_cast<uintptr_t>(this), stack_top, CallProcess, context);
}

extern "C" void activate_context_and_return(
    const EventManager::EventContext& context);

void EventManager::ActivateContext(const EventContext& context) {
  SpawnLocal([this, context]() {
    kprintf("TODO: free existing stack\n");
    stack_ = context.stack;
    auto stack_top = pfn_to_addr(context.stack + STACK_NPAGES);
    my_cpu().set_event_stack(stack_top);
    activate_context_and_return(context);
  });
}

uint8_t EventManager::AllocateVector(std::function<void()> func) {
  auto vec = vector_idx_.fetch_add(1, std::memory_order_relaxed);
  vector_map_.emplace(vec, std::move(func));
  return vec;
}

void EventManager::ProcessInterrupt(int num) {
  apic_eoi();
  auto it = vector_map_.find(num);
  if (it != vector_map_.end()) {
    auto& f = it->second;
    invoke_function(f);
  }
  Process();
}
