ossimctl_d := $(d)ossimctl/
ossimctl_b := $(b)ossimctl/

.PHONY: ossimctl
ossimctl: install-lib
	$(MAKE) O=$(ossimctl_b) PREFIX=$(PREFIX) -C $(ossimctl_d) all

.PHONY: install-ossimctl
install-ossimctl: ossimctl
	$(MAKE) O=$(ossimctl_b) PREFIX=$(PREFIX) -C $(ossimctl_d) install

.PHONY: clean-ossimctl
clean-ossimctl:
	$(MAKE) O=$(ossimctl_b) -C $(ossimctl_d) clean
