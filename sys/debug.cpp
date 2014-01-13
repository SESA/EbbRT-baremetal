#include <sys/console.hpp>
#include <sys/debug.hpp>

namespace ebbrt {
void kvprintf(const char *__restrict format, va_list va) {
  va_list va2;
  va_copy(va2, va);
  auto len = vsnprintf(nullptr, 0, format, va);
  char buffer[len + 1];
  vsnprintf(buffer, len + 1, format, va2);
  console_write(buffer);
}
void kprintf(const char *__restrict format, ...) {
  va_list ap;
  va_start(ap, format);
  kvprintf(format, ap);
  va_end(ap);
}
}
