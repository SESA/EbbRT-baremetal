CXX = $(src)/ext/toolchain/bin/x86_64-pc-ebbrt-g++
CC = $(src)/ext/toolchain/bin/x86_64-pc-ebbrt-gcc

INCLUDES = -I $(src)
INCLUDES += -I $(src)/ext/acpica/include
INCLUDES += -I $(src)/ext/boost/include
INCLUDES += -I $(src)/ext/lwip/include
INCLUDES += -I $(src)/ext/tbb/include
INCLUDES += -iquote $(src)/sys
INCLUDES += -iquote $(src)/ext/lwip/include/ipv4/

CPPFLAGS = -U ebbrt -MD -MT $@ -MP $(optflags) -Wall -Werror \
	-fno-stack-protector $(INCLUDES)
CXXFLAGS = -std=gnu++11
CFLAGS = -std=gnu99
ASFLAGS = -MD -MT $@ -MP $(optflags) -DASSEMBLY

quiet = $(if $V, $1, @echo " $2"; $1)
very-quiet = $(if $V, $1, @$1)

makedir = $(call very-quiet, mkdir -p $(dir $@))
build-cxx = $(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<
q-build-cxx = $(call quiet, $(build-cxx), CXX $@)
build-c = $(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<
q-build-c = $(call quiet, $(build-c), CC $@)
build-s = $(CXX) $(CPPFLAGS) $(ASFLAGS) -c -o $@ $<
q-build-s = $(call quiet, $(build-s), AS $@)

%.o: %.cpp
	$(makedir)
	$(q-build-cxx)

%.o: %.c
	$(makedir)
	$(q-build-c)

%.o: %.S
	$(makedir)
	$(q-build-s)

objects = sys/acpi.o
objects += sys/apic.o
objects += sys/boot.o
objects += sys/clock.o
objects += sys/cpu.o
objects += sys/cpuid.o
objects += sys/debug.o
objects += sys/e820.o
objects += sys/early_page_allocator.o
objects += sys/ebb_allocator.o
objects += sys/event_manager.o
objects += sys/idt.o
objects += sys/isr.o
objects += sys/local_id_map.o
objects += sys/main.o
objects += sys/mem_map.o
objects += sys/net.o
objects += sys/numa.o
objects += sys/page_allocator.o
objects += sys/pci.o
#objects += sys/power.o
objects += sys/slab_allocator.o
objects += sys/smp.o
objects += sys/stack.o
objects += sys/timer.o
objects += sys/tls.o
objects += sys/trans.o
objects += sys/UART_8250.o
objects += sys/virtio_net.o
objects += sys/vmem.o
objects += sys/vmem_allocator.o
objects += $(acpi_objects)
objects += $(tbb_objects)
objects += $(lwip_objects)

tbb_sources := $(shell find $(src)/ext/tbb -type f -name '*.cpp')
tbb_objects = $(patsubst $(src)/%.cpp, %.o, $(tbb_sources))

$(tbb_objects): CPPFLAGS += -iquote $(src)/ext/tbb/include

acpi_sources := $(shell find $(src)/ext/acpica/components -type f -name '*.c')
acpi_objects = $(patsubst $(src)/%.c, %.o, $(acpi_sources))

$(acpi_objects): CFLAGS += -fno-strict-aliasing -Wno-strict-aliasing \
	-Wno-unused-but-set-variable -DACPI_LIBRARY

lwip_sources := $(filter-out %icmp6.c %inet6.c %ip6_addr.c %ip6.c,$(shell find $(src)/ext/lwip -type f -name '*.c'))
lwip_objects = $(patsubst $(src)/%.c, %.o, $(lwip_sources))

$(lwip_objects): CFLAGS += -Wno-address

runtime_objects = sys/newlib.o sys/gthread.o sys/runtime.o

all: ebbrt.iso
.PHONY: all clean

strip = strip -s $< -o $@
mkrescue = grub-mkrescue -o $@ -graft-points boot/ebbrt=$< \
	boot/grub/grub.cfg=$(src)/misc/grub.cfg


ebbrt.iso: ebbrt.elf.stripped
	$(call quiet, $(mkrescue), MKRESCUE $@)

ebbrt.elf.stripped: ebbrt.elf
	$(call quiet, $(strip), STRIP $@)

# ebbrt.elf32: ebbrt.elf
# 	$(call quiet,objcopy -O elf32-i386 $< $@, OBJCOPY $@)

LDFLAGS := -Wl,-n,-z,max-page-size=0x1000 $(optflags)
ebbrt.elf: $(objects) sys/ebbrt.ld $(runtime_objects)
	$(call quiet, $(CXX) $(LDFLAGS) -o $@ $(objects) \
		-T $(src)/sys/ebbrt.ld $(runtime_objects), LD $@)

clean:
	-$(RM) $(wildcard $(objects) ebbrt.elf)

-include $(shell find -name '*.d')
