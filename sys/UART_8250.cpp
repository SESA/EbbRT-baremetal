#include <mutex>

#include <sys/explicitly_constructed.hpp>
#include <sys/io.hpp>
#include <sys/spinlock.hpp>

namespace ebbrt {
constexpr uint16_t PORT = 0x3f8;
/* when DLAB = 0 */
constexpr uint16_t DATA_REG = 0;
constexpr uint16_t INT_ENABLE = 1;
/* when DLAB = 1 : */
constexpr uint16_t BAUD_DIV_LSB = 0; /* Least-signficant byte of the
                                               buad rate divsior */
constexpr uint16_t BAUD_DIV_MSB = 1; /* Most-signficant byte of the
                                               buad rate divsior */

constexpr uint16_t LINE_CNTL_REG = 3;
constexpr uint8_t LINE_CNTL_REG_CHARLEN_8 = 1 << 0 | 1 << 1;
constexpr uint8_t LINE_CNTL_REG_DLAB = 1 << 7;

constexpr uint16_t LINE_STATUS_REG = 5;
constexpr uint8_t LINE_STATUS_REG_THR_EMPTY = 1 << 5;

namespace {
explicitly_constructed<spinlock> console_lock;
}

void console_init() {
  out8(PORT + INT_ENABLE, 0); // disable interrupts

  out8(PORT + LINE_CNTL_REG, LINE_CNTL_REG_DLAB); // enable dlab
  // set divisor to 1 (115200 baud)
  out8(PORT + BAUD_DIV_LSB, 1);
  out8(PORT + BAUD_DIV_MSB, 0);

  // set as 8N1 (8 bits, no parity, one stop bit)
  out8(PORT + LINE_CNTL_REG, LINE_CNTL_REG_CHARLEN_8);

  console_lock.construct();
}

namespace {
void write_locked(char c) {
  while (!(in8(PORT + LINE_STATUS_REG) & LINE_STATUS_REG_THR_EMPTY))
    ;

  out8(PORT + DATA_REG, c);
}
}

void console_write(const char *str) {
  std::lock_guard<spinlock> lock(*console_lock);
  while (*str != '\0') {
    write_locked(*str++);
  }
}
}
