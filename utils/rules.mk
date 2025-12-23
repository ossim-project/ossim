bridge_script := $(d)bridge.py
nat_script := $(d)nat.py

.PHONY: setup-bridges cleanup-bridges setup-nat cleanup-nat

INTERNET_IF ?= eno1
MANAGEMENT_BRIDGE ?= br-ossim0
MANAGEMENT_BRIDGE_CIDR ?= 10.10.10.1/24
PROVIDER_BRIDGE ?= br-ossim1
PROVIDER_BRIDGE_CIDR := 10.10.11.1/24

setup-bridges:
	$(SUDO) python3 $(bridge_script) setup \
		--bridge-if $(MANAGEMENT_BRIDGE) \
		--bridge-cidr $(MANAGEMENT_BRIDGE_CIDR) \
		--prefix $(PREFIX)
	$(SUDO) python3 $(bridge_script) setup \
		--bridge-if $(PROVIDER_BRIDGE) \
		--bridge-cidr $(PROVIDER_BRIDGE_CIDR) \
		--prefix $(PREFIX)
	$(SUDO) modprobe br_netfilter || true
	$(SUDO) sysctl -w net.bridge.bridge-nf-call-iptables=0 || true
	$(SUDO) sysctl -w net.bridge.bridge-nf-call-ip6tables=0 || true
	$(SUDO) sysctl -w net.bridge.bridge-nf-call-arptables=0 || true

cleanup-bridges:
	$(SUDO) python3 $(bridge_script) cleanup --bridge-if $(MANAGEMENT_BRIDGE)
	$(SUDO) python3 $(bridge_script) cleanup --bridge-if $(PROVIDER_BRIDGE)

setup-nat:
	$(SUDO) python3 $(nat_script) setup \
		--bridge-if $(MANAGEMENT_BRIDGE) \
		--internet-if $(INTERNET_IF)
	$(SUDO) python3 $(nat_script) setup \
		--bridge-if $(PROVIDER_BRIDGE) \
		--internet-if $(INTERNET_IF)

cleanup-nat:
	$(SUDO) python3 $(nat_script) cleanup \
		--bridge-if $(MANAGEMENT_BRIDGE) \
		--internet-if $(INTERNET_IF)
	$(SUDO) python3 $(nat_script) cleanup \
		--bridge-if $(PROVIDER_BRIDGE) \
		--internet-if $(INTERNET_IF)
