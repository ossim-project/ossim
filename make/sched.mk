SCX_VERSION := 1.0.16

sched_d := $(d)sched/
sched_b := $(b)sched/

scx_dir := $(sched_b)scx/
 
$(scx_dir):
	rm -rf $@
	git clone https://github.com/sched-ext/scx.git --branch v$(SCX_VERSION) $@

.PHONY: init-scx
init-scx:
	@rm -rf $(scx_dir)
	$(MAKE) $(scx_dir)

.PHONY: sched
sched: $(scx_dir)
	$(MAKE) O=$(sched_b) SCX_SRC_DIR=$(scx_dir) -C $(sched_d) all

.PHONY: clean-sched
clean-sched:
	$(MAKE) O=$(sched_b) SCX_SRC_DIR=$(scx_dir) -C $(sched_d) clean

.PHONY: run-sched
run-sched:
	$(MAKE) O=$(sched_b) SCX_SRC_DIR=$(scx_dir) -C $(sched_d) run-scx_ossim

.PHONY: format-sched
format-sched:
	$(CLANG_FORMAT) -i --style=file $(shell find $(sched_d) -name '*.c' -or -name '*.h')
