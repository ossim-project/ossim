export OS_PROJECT_DOMAIN_NAME=Default
export OS_USER_DOMAIN_NAME=Default
export OS_PROJECT_NAME={{ .openstack.id.nonadmin.project }}
export OS_USERNAME={{ .openstack.id.nonadmin.user.name }}
export OS_PASSWORD={{ .openstack.id.nonadmin.user.pass }}
export OS_AUTH_URL=http://controller:5000/v3
export OS_IDENTITY_API_VERSION=3
export OS_IMAGE_API_VERSION=2
