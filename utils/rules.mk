bridge_script := $(d)bridge.sh
nat_script := $(d)nat.sh

.PHONY: setup-bridges cleanup-bridges setup-nat cleanup-nat

INTERNET_IF ?= eno1
MANAGEMENT_BRIDGE ?= br-ossim0
MANAGEMENT_BRIDGE_CIDR ?= 10.10.10.1/24
PROVIDER_BRIDGE ?= br-ossim1
PROVIDER_BRIDGE_CIDR := 10.10.11.1/24

setup-bridges:
	INTERNET_IF=$(INTERNET_IF) \
	BRIDGE_IF=$(MANAGEMENT_BRIDGE) \
	BRIDGE_IF_CIDR=$(MANAGEMENT_BRIDGE_CIDR) \
	sudo -E bash $(bridge_script) setup
	INTERNET_IF=$(INTERNET_IF) \
	BRIDGE_IF=$(PROVIDER_BRIDGE) \
	BRIDGE_IF_CIDR=$(PROVIDER_BRIDGE_CIDR) \
	sudo -E bash $(bridge_script) setup

cleanup-bridges:
	INTERNET_IF=$(INTERNET_IF) \
	BRIDGE_IF=$(MANAGEMENT_BRIDGE) \
	BRIDGE_IF_CIDR=$(MANAGEMENT_BRIDGE_CIDR) \
	sudo -E bash $(bridge_script) cleanup 
	INTERNET_IF=$(INTERNET_IF) \
	BRIDGE_IF=$(PROVIDER_BRIDGE) \
	BRIDGE_IF_CIDR=$(PROVIDER_BRIDGE_CIDR) \
	sudo -E bash $(bridge_script) cleanup 

setup-nat:
	INTERNET_IF=$(INTERNET_IF) \
	BRIDGE_IF=$(MANAGEMENT_BRIDGE) \
	sudo -E bash $(nat_script) setup
	INTERNET_IF=$(INTERNET_IF) \
	BRIDGE_IF=$(PROVIDER_BRIDGE) \
	sudo -E bash $(nat_script) setup

cleanup-nat:
	INTERNET_IF=$(INTERNET_IF) \
	BRIDGE_IF=$(MANAGEMENT_BRIDGE) \
	sudo -E bash $(nat_script) cleanup
	INTERNET_IF=$(INTERNET_IF) \
	BRIDGE_IF=$(PROVIDER_BRIDGE) \
	sudo -E bash $(nat_script) cleanup
