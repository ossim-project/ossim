# Disk images output directory

bigdata_img_o := $(o)
bigdata_dimg_o := $(o)disks/
bigdata_dimgs :=

$(eval $(call include_rules,$(d)base/rules.mk))
$(eval $(call include_rules,$(d)nodes/rules.mk))
