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
