#pragma once

#include <stack>

#include <sys/main.hpp>
#include <sys/smp.hpp>
#include <sys/trans.hpp>
#include <sys/vmem_allocator.hpp>

namespace ebbrt {

class EventManager {
  friend void ebbrt::kmain(ebbrt::MultibootInformation *mbi);
  friend void ebbrt::smp_main();
  void StartLoop() __attribute__((noreturn));
  static void CallLoop(uintptr_t mgr) __attribute__((noreturn));
  void Loop() __attribute__((noreturn));

  class EventContext {
    pfn_t stack_;
  public:
    EventContext();
    uintptr_t top_of_stack() const;
  };
  EventContext active_context_;
  std::stack<std::function<void()> > tasks_;
public:
  static void Init();
  static EventManager &HandleFault(EbbId id);

  void SpawnLocal(std::function<void()> func);
};

constexpr auto event_manager = EbbRef<EventManager>{ event_manager_id };
}
