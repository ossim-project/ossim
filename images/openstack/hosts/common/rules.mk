ubuntu_phase2_common_input := $(shell find $(d)phase2/input/ -type f | sed -e 's|^$(d)phase2/input/||' -e 's|.tpl$$||')

ubuntu_phase2_install_script := $(d)phase2/install.sh
