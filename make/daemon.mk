daemon_d := $(d)daemon/
deamon_b := $(b)daemon/

.PHONY: build-daemon
build-daemon:
	$(MAKE) O=$(deamon_b) -C $(daemon_d) all

.PHONY: clean-daemon
clean-daemon:
	$(MAKE) O=$(deamon_b) -C $(daemon_d) clean

.PHONY: run-daemon
run-daemon:
	$(MAKE) O=$(deamon_b) -C $(daemon_d) run

.PHONY: format-daemon
format-daemon:
	$(CLANG_FORMAT) -i --style=file $(shell find $(daemon_d) -name '*.c' -or -name '*.h')
