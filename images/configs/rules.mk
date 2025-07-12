yq := yq

platform_config_deps := $(b)platform.sed $(d)platform.yaml
openstack_config_deps :=  $(b)openstack.sed $(d)openstack.yaml
misc_config_deps :=  $(b)misc.sed $(d)misc.yaml
host_config_deps :=  $(b)host.sed $(d)host.yaml
workload_config_deps := $(b)workload.sed $(d)workload.yaml

config_yaml_all := $(d)misc.yaml $(d)platform.yaml $(d)openstack.yaml

config_yaml := $(o)config.yaml
config_sed := $(b)config.sed
config_deps := $(config_yaml) $(config_sed)

$(config_yaml): $(config_yaml_all) 
	mkdir -p $(@D)
	$(yq) eval-all 'select(fileIndex == 0) * select(fileIndex > 0) | explode(.)' $(config_yaml_all) > $@

# $(1) - .yaml $(2) - .sed
define yaml2sed
sed 's/\//\\\//g' $(1) | $(yq) e 'explode(.) | .. | select(. == "*") | "s/{{ ." + (path | join(".")) + " }}/" + . + "/g"' > $(2)
endef

$(config_sed): $(config_yaml) 
	mkdir -p $(@D)
	$(call yaml2sed,$<,$@)

$(b)%.sed: $(d)%.yaml 
	mkdir -p $(@D)
	$(call yaml2sed,$<,$@)

config_d := $(d)
config_b := $(b)

# Get config value from a specified config file
define conffget
$(shell $(yq) eval 'explode(.) | .$(1)$(2)' $(config_d)$(1).yaml)
endef

define confget
$(shell $(yq) eval 'explode(.) | $(1)' $(config_yaml))
endef

# Get config value from a specified config file
define conffsed
sed -f $(config_b)$(1).sed $(2) > $(3)
endef

define confsed
sed -f $(config_sed) $(1) > $(2)
endef
