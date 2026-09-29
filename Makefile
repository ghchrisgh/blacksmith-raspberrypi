CC = gcc
CFLAGS = -Wall -Wextra -std=c11
.PHONY: run clean build-pmu-kernel-module test-pmu load-pmu unload-pmu check venv



build-pmu-kernel-module: ko/enable_arm_pmu.ko

ko/enable_arm_pmu.ko: enable_arm_pmu.c
	@mkdir -p ko
	@echo "obj-m := enable_arm_pmu.o" > ko/Makefile
	@cp enable_arm_pmu.c ko/
	@echo "Building ARM PMU kernel module"
	@make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/ko modules

load-pmu: build-pmu-kernel-module
	@echo "Loading ARM PMU kernel module"
	@sudo ./load-module
	@rm -rf ko
	@dmesg | tail

unload-pmu:
	@echo "Unloading ARM PMU kernel module"
	@echo sudo ./unload-module
	@sudo ./unload-module
	@dmesg | tail



TEST_DIR = experiments/test-pmu
test-pmu: $(TEST_DIR)/perf_arm_pmu $(TEST_DIR)/perf_event_open
	@echo "Running perf_arm_pmu test"
	@./$(TEST_DIR)/perf_arm_pmu 64
	@echo "Running perf_event_open test"
	@./$(TEST_DIR)/perf_event_open 64
	rm -rf experiments/test-pmu/perf_arm_pmu experiments/test-pmu/perf_event_open

$(TEST_DIR)/perf_arm_pmu: $(TEST_DIR)/perf_arm_pmu.c
	@echo "Compiling perf_arm_pmu"
	@$(CC) -O3 -std=gnu99 $< -o $@

$(TEST_DIR)/perf_event_open: $(TEST_DIR)/perf_event_open.c
	@echo "Compiling perf_event_open"
	@$(CC) -O3 -std=gnu99 $< -o $@



check:
	@echo "Checking Python version and dependencies..."
	@python3 check_dependencies.py || ( \
		read -p "Install missing dependencies with pip? (y/n): " yn; \
		if [ "$$yn" = "y" ] || [ "$$yn" = "Y" ]; then \
			pip3 install -r requirements.txt; \
		else \
			echo "Aborted"; exit 1; \
		fi \
	)



venv:
	@./setup_venv.sh



setup: 
	@./performance.sh performance



clean: 
	rm -rf ko
	rm -rf experiments/test-pmu/perf_arm_pmu experiments/test-pmu/perf_event_open