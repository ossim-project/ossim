local:
  hostname: controller
  network:
    management:
      ip: {{ .openstack.network.management.hosts.controller.ip }}
    provider:
      ip: {{ .openstack.network.provider.hosts.controller.ip }}      