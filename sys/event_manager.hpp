#pragma once

#include <stack>
#include <unordered_map>

#include <sys/main.hpp>
#include <sys/smp.hpp>
#include <sys/trans.hpp>
#include <sys/vmem_allocator.hpp>

namespace ebbrt {

extern "C" void event_interrupt(int num);

class EventManager {
  friend void ebbrt::kmain(ebbrt::MultibootInformation* mbi);
  friend void ebbrt::smp_main();
  void StartProcessingEvents() __attribute__((noreturn));
  static void CallProcess(uintptr_t mgr) __attribute__((noreturn));
  void Process() __attribute__((noreturn));
  friend void ebbrt::event_interrupt(int num);
  void ProcessInterrupt(int num) __attribute__((noreturn));

  pfn_t AllocateStack();

  pfn_t stack_;
  std::stack<pfn_t> free_stacks_;
  std::stack<std::function<void()> > tasks_;
  std::unordered_map<uint8_t, std::function<void()> > vector_map_;
  std::atomic<uint8_t> vector_idx_;

 public:
  static void Init();
  static EventManager& HandleFault(EbbId id);

  EventManager();

  void SpawnLocal(std::function<void()> func);
  struct EventContext {
    uint64_t rbx;
    uint64_t rsp;
    uint64_t rbp;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    pfn_t stack;
  };
  void SaveContext(EventContext& context);
  void ActivateContext(const EventContext& context);
  uint8_t AllocateVector(std::function<void()> func);
};

constexpr auto event_manager = EbbRef<EventManager>(event_manager_id);
}
