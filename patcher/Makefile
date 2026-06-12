# gnu-efi location is resolved portably via pkg-config (provided by the Nix dev
# shell — run `nix develop` first). Override on the command line if needed:
#   make GNU_EFI=/path/to/gnu-efi
GNU_EFI ?= $(shell pkg-config --variable=prefix gnu-efi 2>/dev/null)

ifeq ($(GNU_EFI),)
$(error gnu-efi not found. Run inside `nix develop`, or set GNU_EFI=/path/to/gnu-efi)
endif

CC       := gcc
LD       := ld
OBJCOPY  := objcopy

EFI_INCLUDE := $(GNU_EFI)/include/efi
EFI_LIB     := $(GNU_EFI)/lib
EFI_LDS     := $(EFI_LIB)/elf_x86_64_efi.lds
EFI_CRT     := $(EFI_LIB)/crt0-efi-x86_64.o
GNUEFI_A    := $(EFI_LIB)/libgnuefi.a
EFI_A       := $(EFI_LIB)/libefi.a

# Compile for SysV ABI (default).  crt0-efi-x86_64.o handles the MS-ABI→SysV
# translation at the UEFI entry point.  uefi_call_wrapper() converts SysV→MS
# for every direct UEFI protocol call.  gnu-efi library code (InitializeLib,
# Print …) was compiled with SysV — do NOT use -mabi=ms here.
CFLAGS := \
    -I$(EFI_INCLUDE) -I$(EFI_INCLUDE)/x86_64 \
    -fpic \
    -fshort-wchar \
    -mno-red-zone \
    -fno-stack-protector \
    -ffreestanding \
    -fno-strict-aliasing \
    -Wall -Wextra \
    -O2

LDFLAGS := \
    -nostdlib \
    -znocombreloc \
    -shared \
    -Bsymbolic \
    -T $(EFI_LDS)

OBJCOPY_FLAGS := \
    -I elf64-x86-64 \
    -O pei-x86-64 \
    --subsystem=10 \
    -j .text \
    -j .sdata \
    -j .data \
    -j .rodata \
    -j .dynamic \
    -j .dynsym \
    -j .rel \
    -j .rela \
    -j .reloc \
    --strip-debug

.PHONY: all clean

all: BiosAdvancedPatch.efi

BiosAdvancedPatch.o: BiosAdvancedPatch.c
	$(CC) $(CFLAGS) -c -o $@ $<

BiosAdvancedPatch.so: BiosAdvancedPatch.o
	$(LD) $(LDFLAGS) $(EFI_CRT) $< $(GNUEFI_A) $(EFI_A) -o $@

BiosAdvancedPatch.efi: BiosAdvancedPatch.so
	$(OBJCOPY) $(OBJCOPY_FLAGS) $< $@
	@echo "" && sha256sum $@ && echo "" && \
	python3 -c "\
import struct; \
data = open('$@','rb').read(); \
pe = struct.unpack_from('<I', data, 0x3c)[0]; \
print('PE magic:', data[pe:pe+4].hex()); \
mach = struct.unpack_from('<H', data, pe+4)[0]; \
print('Machine: 0x%x (expect 0x8664)' % mach); \
subsys = struct.unpack_from('<H', data, pe+4+20+68)[0]; \
print('Subsystem:', subsys, '(expect 10)'); \
print('File size:', len(data), 'bytes') \
"

clean:
	rm -f *.o *.so *.efi
