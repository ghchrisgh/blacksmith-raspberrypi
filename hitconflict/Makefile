# Row-Hit and Row-Conflict and DRAM reverse engineering
CXX      ?= g++
# -MMD -MP
CXXFLAGS  = -O2 -std=c++17 -I$(PWD)/asmjit -Isrc -MMD -MP
LDFLAGS   = -L$(PWD)/asmjit/build -lasmjit -Wl,-rpath,$(PWD)/asmjit/build
GB_PAGE  = /sys/kernel/mm/hugepages/hugepages-1048576kB/nr_hugepages
HUGEPAGE = /mnt/huge
CSV      = accesses.csv
BUILD    = obj

CONFIG = src/config.h

# -----------------------------------------------------------------------
# Common objects (shared between both binaries)
# -----------------------------------------------------------------------
COMMON_SRCS = src/helper.cpp src/hugepage.cpp src/pagemap.cpp \
              src/jit_kernel.cpp src/util.cpp
COMMON_OBJS = $(patsubst src/%.cpp,$(BUILD)/%.o,$(COMMON_SRCS))

HIST_OUT = $(BUILD)/hist
EFN_OUT  = $(BUILD)/efn

# .d
-include $(COMMON_OBJS:.o=.d)
-include $(BUILD)/hist_main.d
-include $(BUILD)/efn_main.d

# -----------------------------------------------------------------------
# Build AsmJIT
# -----------------------------------------------------------------------
build_asmjit:
	@echo "Build AsmJIT..."
	@cmake -S asmjit -B asmjit/build -DCMAKE_BUILD_TYPE=Release \
		-DASMJIT_STATIC=OFF
	@cmake --build asmjit/build --parallel

# -----------------------------------------------------------------------
# Compile common objects
# -----------------------------------------------------------------------
$(BUILD)/%.o: src/%.cpp $(CONFIG) | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

# -----------------------------------------------------------------------
# Build both binaries (default)
# -----------------------------------------------------------------------
all: build_asmjit $(HIST_OUT) $(EFN_OUT)

# Binary 1: hist
$(HIST_OUT): $(COMMON_OBJS) $(BUILD)/hist_main.o
	$(CXX) $^ -o $@ $(LDFLAGS)

# Binary 2: efn
$(EFN_OUT): $(COMMON_OBJS) $(BUILD)/efn_main.o
	$(CXX) $^ -o $@ $(LDFLAGS)

# -----------------------------------------------------------------------
# Hugepage Setup (1 GB)
# -----------------------------------------------------------------------
setup:
	@echo "Configure 1-GB-Hugepage..."
	echo 1 | sudo tee $(GB_PAGE)
	@if ! mountpoint -q $(HUGEPAGE); then \
		sudo mkdir -p $(HUGEPAGE); \
		sudo mount -t hugetlbfs -o pagesize=1G none $(HUGEPAGE); \
		sudo chown $(USER):$(USER) $(HUGEPAGE); \
	fi
	@echo "Hugepage ready: $$(cat $(GB_PAGE))"

# -----------------------------------------------------------------------
# Hugepage Teardown
# -----------------------------------------------------------------------
teardown:
	@if mountpoint -q $(HUGEPAGE); then \
		sudo umount $(HUGEPAGE); \
	fi
	@if [ -d $(HUGEPAGE) ]; then \
		sudo rmdir $(HUGEPAGE); \
	fi
	echo 0 | sudo tee $(GB_PAGE)

# -----------------------------------------------------------------------
# Run the binaries
# -----------------------------------------------------------------------
run_hist:
	sudo LD_LIBRARY_PATH=$(PWD)/asmjit/build:$$LD_LIBRARY_PATH \
		$(HIST_OUT)

run_efn:
	sudo LD_LIBRARY_PATH=$(PWD)/asmjit/build:$$LD_LIBRARY_PATH \
		$(EFN_OUT)

# -----------------------------------------------------------------------
# Create histogram, plot
# -----------------------------------------------------------------------
analyze:
	python3 py/analyze_dram.py dram_map.json

py:
	python3 py/histogram.py $(CSV)

# -----------------------------------------------------------------------
# Clean
# -----------------------------------------------------------------------
clean:
	rm -rf $(BUILD) $(CSV)

cleanall: clean
	rm -f *.csv *.png *.json

.PHONY: build_asmjit all setup teardown run_hist run_efn py analyze clean cleanall

meas:
	python3 py/efn_meas_hist.py efn_meas.csv
