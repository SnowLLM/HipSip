OUT       ?= build
ROCM_PATH ?= /opt/rocm
ROCM_LLVM ?= $(ROCM_PATH)/llvm/bin

CLANG   := $(ROCM_LLVM)/clang
DLLTOOL := $(ROCM_LLVM)/llvm-dlltool

# Wine loads the exact DLL name in the PE import table.
DLLS := amdhip64 amdhip64_6 amdhip64_7

COMMON := -O2 -Wall -Wextra -iquote . -MMD -MP

# Use the MSVC runtime libraries from the sysroot when linking PE binaries.
WINFLAGS := --target=x86_64-pc-windows-msvc -rtlib=platform \
	-isystem $(XWIN)/crt/include -isystem $(XWIN)/sdk/include/ucrt \
	-isystem $(XWIN)/sdk/include/um -isystem $(XWIN)/sdk/include/shared \
	-L$(XWIN)/crt/lib/x86_64 -L$(XWIN)/sdk/lib/ucrt/x86_64 -L$(XWIN)/sdk/lib/um/x86_64

xwin = @test -n "$(XWIN)" || { echo "set XWIN to an xwin sysroot (crt/ and sdk/)" >&2; exit 1; }

DEFS := $(DLLS:%=$(OUT)/%.def)
LIBS := $(DLLS:%=$(OUT)/%.lib)
BINS := $(OUT)/libhipsip.so $(DLLS:%=$(OUT)/%.dll) $(OUT)/hip_probe.exe

all: $(BINS) $(DEFS) $(LIBS)
	@echo "built: $(BINS)"

$(OUT):
	mkdir -p $@

$(OUT)/libhipsip.so: unix/hipsip.c | $(OUT)
	gcc -shared -fPIC $(COMMON) -I$(ROCM_PATH)/include -D__HIP_PLATFORM_AMD__ $< -o $@ -ldl

# The module definition supplies exports for functions declared in the ROCm headers.
$(OUT)/%.def: hip_symbols.def | $(OUT)
	{ echo "LIBRARY $*.dll"; echo "EXPORTS"; \
	  sed -n 's/^HIP_FN([^,]*, *\([A-Za-z0-9_]*\).*/\1/p' $<; } > $@

# lld-link and dlltool both write the import library. Keep them in a single recipe.
$(OUT)/%.dll $(OUT)/%.lib &: pe/amdhip64.c $(OUT)/%.def | $(OUT)
	$(xwin)
	$(CLANG) $(COMMON) -D_CRT_SECURE_NO_WARNINGS -isystem $(ROCM_PATH)/include $(WINFLAGS) \
	    -shared $< -o $(OUT)/$*.dll -Xlinker /DEF:$(OUT)/$*.def
	$(DLLTOOL) -m i386:x86-64 -d $(OUT)/$*.def -l $(OUT)/$*.lib

$(OUT)/hip_probe.exe: probe/hip_probe.c $(OUT)/amdhip64.lib | $(OUT)
	$(xwin)
	$(CLANG) -O2 -Wall -iquote . -MMD -MP $(WINFLAGS) $< $(OUT)/amdhip64.lib -o $@

ifneq ($(GPU_ARCH),)
KERNEL_TEST := $(OUT)/test_kernel_launch_$(GPU_ARCH).exe
$(KERNEL_TEST): tests/kernel_launch.hip $(OUT)/amdhip64.lib | $(OUT)
	$(xwin)
	$(ROCM_LLVM)/clang++ $(WINFLAGS) -O2 --rocm-path=$(ROCM_PATH) --offload-arch=$(GPU_ARCH) \
	    $< -L$(OUT) -o $@
endif

check: all $(KERNEL_TEST)
	OUT="$(abspath $(OUT))" ./run.sh "$(OUT)/hip_probe.exe"
ifneq ($(GPU_ARCH),)
	OUT="$(abspath $(OUT))" ./run.sh "$(KERNEL_TEST)"
endif

clean:
	rm -rf $(OUT)

-include $(wildcard $(OUT)/*.d)

.PHONY: all check clean
