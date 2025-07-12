export ADMIN_PASS={{ .openstack.id.admin_pass }}
export GLANCE_PASS={{ .openstack.id.glance_pass }}
export PLACEMENT_PASS={{ .openstack.id.placement_pass }}
export NOVA_PASS={{ .openstack.id.nova_pass }}
export NEUTRON_PASS={{ .openstack.id.neutron_pass }}
export RABBIT_PASS={{ .openstack.id.rabbit_pass }}
export METADATA_SECRET={{ .openstack.id.metadata_secret }}

export KEYSTONE_DBPASS={{ .openstack.id.keystone_dbpass }}
export GLANCE_DBPASS={{ .openstack.id.glance_dbpass }}
export PLACEMENT_DBPASS={{ .openstack.id.placement_dbpass }}
export NOVA_DBPASS={{ .openstack.id.nova_dbpass }}
export NEUTRON_DBPASS={{ .openstack.id.neutron_dbpass }}

export USER_PROJECT={{ .openstack.id.nonadmin.project }}
export USER_NAME={{ .openstack.id.nonadmin.user.name }}
export USER_PASS={{ .openstack.id.nonadmin.user.pass }}
export USER_ROLE={{ .openstack.id.nonadmin.role }}
