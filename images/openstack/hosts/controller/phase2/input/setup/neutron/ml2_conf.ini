[DEFAULT]

#
# From oslo.log
#

# If set to true, the logging level will be set to DEBUG instead of the default
# INFO level. (boolean value)
# Note: This option can be changed without restarting.
#debug = false

# The name of a logging configuration file. This file is appended to any
# existing logging configuration files. For details about logging configuration
# files, see the Python logging module documentation. Note that when logging
# configuration files are used then all logging configuration is set in the
# configuration file and other logging configuration options are ignored (for
# example, log-date-format). (string value)
# Note: This option can be changed without restarting.
# Deprecated group/name - [DEFAULT]/log_config
#log_config_append = <None>

# Defines the format string for %%(asctime)s in log records. Default:
# %(default)s . This option is ignored if log_config_append is set. (string
# value)
#log_date_format = %Y-%m-%d %H:%M:%S

# (Optional) Name of log file to send logging output to. If no default is set,
# logging will go to stderr as defined by use_stderr. This option is ignored if
# log_config_append is set. (string value)
# Deprecated group/name - [DEFAULT]/logfile
#log_file = <None>

# (Optional) The base directory used for relative log_file  paths. This option
# is ignored if log_config_append is set. (string value)
# Deprecated group/name - [DEFAULT]/logdir
#log_dir = <None>

# Uses logging handler designed to watch file system. When log file is moved or
# removed this handler will open a new log file with specified path
# instantaneously. It makes sense only if log_file option is specified and
# Linux platform is used. This option is ignored if log_config_append is set.
# (boolean value)
#watch_log_file = false

# Use syslog for logging. Existing syslog format is DEPRECATED and will be
# changed later to honor RFC5424. This option is ignored if log_config_append
# is set. (boolean value)
#use_syslog = false

# Enable journald for logging. If running in a systemd environment you may wish
# to enable journal support. Doing so will use the journal native protocol
# which includes structured metadata in addition to log messages.This option is
# ignored if log_config_append is set. (boolean value)
#use_journal = false

# Syslog facility to receive log lines. This option is ignored if
# log_config_append is set. (string value)
#syslog_log_facility = LOG_USER

# Use JSON formatting for logging. This option is ignored if log_config_append
# is set. (boolean value)
#use_json = false

# Log output to standard error. This option is ignored if log_config_append is
# set. (boolean value)
#use_stderr = false

# DEPRECATED: Log output to Windows Event Log. (boolean value)
# This option is deprecated for removal.
# Its value may be silently ignored in the future.
# Reason: Windows support is no longer maintained.
#use_eventlog = false

# The amount of time before the log files are rotated. This option is ignored
# unless log_rotation_type is set to "interval". (integer value)
#log_rotate_interval = 1

# Rotation interval type. The time of the last file change (or the time when
# the service was started) is used when scheduling the next rotation. (string
# value)
# Possible values:
# Seconds - <No description provided>
# Minutes - <No description provided>
# Hours - <No description provided>
# Days - <No description provided>
# Weekday - <No description provided>
# Midnight - <No description provided>
#log_rotate_interval_type = days

# Maximum number of rotated log files. (integer value)
#max_logfile_count = 30

# Log file maximum size in MB. This option is ignored if "log_rotation_type" is
# not set to "size". (integer value)
#max_logfile_size_mb = 200

# Log rotation type. (string value)
# Possible values:
# interval - Rotate logs at predefined time intervals.
# size - Rotate logs once they reach a predefined size.
# none - Do not rotate log files.
#log_rotation_type = none

# Format string to use for log messages with context. Used by
# oslo_log.formatters.ContextFormatter (string value)
#logging_context_format_string = %(asctime)s.%(msecs)03d %(process)d %(levelname)s %(name)s [%(global_request_id)s %(request_id)s %(user_identity)s] %(instance)s%(message)s

# Format string to use for log messages when context is undefined. Used by
# oslo_log.formatters.ContextFormatter (string value)
#logging_default_format_string = %(asctime)s.%(msecs)03d %(process)d %(levelname)s %(name)s [-] %(instance)s%(message)s

# Additional data to append to log message when logging level for the message
# is DEBUG. Used by oslo_log.formatters.ContextFormatter (string value)
#logging_debug_format_suffix = %(funcName)s %(pathname)s:%(lineno)d

# Prefix each line of exception output with this format. Used by
# oslo_log.formatters.ContextFormatter (string value)
#logging_exception_prefix = %(asctime)s.%(msecs)03d %(process)d ERROR %(name)s %(instance)s

# Defines the format string for %(user_identity)s that is used in
# logging_context_format_string. Used by oslo_log.formatters.ContextFormatter
# (string value)
#logging_user_identity_format = %(user)s %(project)s %(domain)s %(system_scope)s %(user_domain)s %(project_domain)s

# List of package logging levels in logger=LEVEL pairs. This option is ignored
# if log_config_append is set. (list value)
#default_log_levels = amqp=WARN,amqplib=WARN,boto=WARN,qpid=WARN,sqlalchemy=WARN,suds=INFO,oslo.messaging=INFO,oslo_messaging=INFO,iso8601=WARN,requests.packages.urllib3.connectionpool=WARN,urllib3.connectionpool=WARN,websocket=WARN,requests.packages.urllib3.util.retry=WARN,urllib3.util.retry=WARN,keystonemiddleware=WARN,routes.middleware=WARN,stevedore=WARN,taskflow=WARN,keystoneauth=WARN,oslo.cache=INFO,oslo_policy=INFO,dogpile.core.dogpile=INFO

# Enables or disables publication of error events. (boolean value)
#publish_errors = false

# The format for an instance that is passed with the log message. (string
# value)
#instance_format = "[instance: %(uuid)s] "

# The format for an instance UUID that is passed with the log message. (string
# value)
#instance_uuid_format = "[instance: %(uuid)s] "

# Interval, number of seconds, of log rate limiting. (integer value)
#rate_limit_interval = 0

# Maximum number of logged messages per rate_limit_interval. (integer value)
#rate_limit_burst = 0

# Log level name used by rate limiting: CRITICAL, ERROR, INFO, WARNING, DEBUG
# or empty string. Logs with level greater or equal to rate_limit_except_level
# are not filtered. An empty string means that all levels are filtered. (string
# value)
#rate_limit_except_level = CRITICAL

# Enables or disables fatal status of deprecations. (boolean value)
#fatal_deprecations = false


[ml2]
type_drivers = flat,vlan,vxlan
tenant_network_types = vxlan
mechanism_drivers = openvswitch,l2population
extension_drivers = port_security

#
# From neutron.ml2
#

# List of network type driver entrypoints to be loaded from the
# neutron.ml2.type_drivers namespace. (list value)
#type_drivers = local,flat,vlan,gre,vxlan,geneve

# Ordered list of network_types to allocate as tenant networks. The default
# value 'local' is useful for single-box testing but provides no connectivity
# between hosts. (list value)
#tenant_network_types = local

# An ordered list of networking mechanism driver entrypoints to be loaded from
# the neutron.ml2.mechanism_drivers namespace. (list value)
#mechanism_drivers =

# An ordered list of extension driver entrypoints to be loaded from the
# neutron.ml2.extension_drivers namespace. For example: extension_drivers =
# port_security,qos (list value)
#extension_drivers =

# Maximum size of an IP packet (MTU) that can traverse the underlying physical
# network infrastructure without fragmentation when using an overlay/tunnel
# protocol. This option allows specifying a physical network MTU value that
# differs from the default global_physnet_mtu value. (integer value)
#path_mtu = 0

# A list of mappings of physical networks to MTU values. The format of the
# mapping is <physnet>:<mtu val>. This mapping allows specifying a physical
# network MTU value that differs from the default global_physnet_mtu value.
# (list value)
#physical_network_mtus =

# Default network type for external networks when no provider attributes are
# specified. By default it is None, which means that if provider attributes are
# not specified while creating external networks then they will have the same
# type as tenant networks. Allowed values for external_network_type config
# option depend on the network type values configured in type_drivers config
# option. (string value)
#external_network_type = <None>

# IP version of all overlay (tunnel) network endpoints. (integer value)
# Possible values:
# 4 - IPv4
# 6 - IPv6
#overlay_ip_version = 4

# Resource provider name for the host with tunnelled networks. This resource
# provider represents the available bandwidth for all tunnelled networks in a
# compute node. NOTE: this parameter is used both by the Neutron server and the
# mechanism driver agents; it is recommended not to change it once any resource
# provider register has been created. (string value)
#tunnelled_network_rp_name = rp_tunnelled


[ml2_type_flat]
flat_networks = provider

#
# From neutron.ml2
#

# List of physical_network names with which flat networks can be created. Use
# default '*' to allow flat networks with arbitrary physical_network names. Use
# an empty list to disable flat networks. (list value)
#flat_networks = *


[ml2_type_geneve]

#
# From neutron.ml2
#

# Comma-separated list of <vni_min>:<vni_max> tuples enumerating ranges of
# Geneve VNI IDs that are available for tenant network allocation. Note OVN
# does not use the actual values. (list value)
#vni_ranges =

# The maximum allowed Geneve encapsulation header size (in bytes). Geneve
# header is extensible, this value is used to calculate the maximum MTU for
# Geneve-based networks. The default is 30, which is the size of the Geneve
# header without any additional option headers. Note the default is not enough
# for OVN which requires at least 38. (integer value)
#max_header_size = 30


[ml2_type_gre]

#
# From neutron.ml2
#

# Comma-separated list of <tun_min>:<tun_max> tuples enumerating ranges of GRE
# tunnel IDs that are available for tenant network allocation (list value)
#tunnel_id_ranges =


[ml2_type_vlan]

#
# From neutron.ml2
#

# List of <physical_network>:<vlan_min>:<vlan_max> or <physical_network>
# specifying physical_network names usable for VLAN provider and tenant
# networks, as well as ranges of VLAN tags on each available for allocation to
# tenant networks. If no range is defined, the whole valid VLAN ID set [1,
# 4094] will be assigned. (list value)
#network_vlan_ranges =


[ml2_type_vxlan]
vni_ranges = 1:1000

#
# From neutron.ml2
#

# Comma-separated list of <vni_min>:<vni_max> tuples enumerating ranges of
# VXLAN VNI IDs that are available for tenant network allocation (list value)
#vni_ranges =

# Multicast group for VXLAN. When configured, will enable sending all broadcast
# traffic to this multicast group. When left unconfigured, will disable
# multicast VXLAN mode. (string value)
#vxlan_group = <None>


[ovn]

#
# From neutron.ml2.ovn
#

# The connection string for the OVN_Northbound OVSDB.
# Use tcp:IP:PORT for TCP connection.
# Use ssl:IP:PORT for SSL connection. The ovn_nb_private_key,
# ovn_nb_certificate and ovn_nb_ca_cert are mandatory.
# Use unix:FILE for unix domain socket connection.
# Multiple connections can be specified by a comma separated string. See also:
# https://github.com/openvswitch/ovs/blob/ab4d3bfbef37c31331db5a9dbe7c22eb8d5e5e5f/python/ovs/db/idl.py#L215-L216
# (string value)
#ovn_nb_connection = tcp:127.0.0.1:6641

# The PEM file with private key for SSL connection to OVN-NB-DB (string value)
#ovn_nb_private_key =

# The PEM file with certificate that certifies the private key specified in
# ovn_nb_private_key (string value)
#ovn_nb_certificate =

# The PEM file with CA certificate that OVN should use to verify certificates
# presented to it by SSL peers (string value)
#ovn_nb_ca_cert =

# The connection string for the OVN_Southbound OVSDB.
# Use tcp:IP:PORT for TCP connection.
# Use ssl:IP:PORT for SSL connection. The ovn_sb_private_key,
# ovn_sb_certificate and ovn_sb_ca_cert are mandatory.
# Use unix:FILE for unix domain socket connection.
# Multiple connections can be specified by a comma separated string. See also:
# https://github.com/openvswitch/ovs/blob/ab4d3bfbef37c31331db5a9dbe7c22eb8d5e5e5f/python/ovs/db/idl.py#L215-L216
# (string value)
#ovn_sb_connection = tcp:127.0.0.1:6642

# The PEM file with private key for SSL connection to OVN-SB-DB (string value)
#ovn_sb_private_key =

# The PEM file with certificate that certifies the private key specified in
# ovn_sb_private_key (string value)
#ovn_sb_certificate =

# The PEM file with CA certificate that OVN should use to verify certificates
# presented to it by SSL peers (string value)
#ovn_sb_ca_cert =

# Timeout, in seconds, for the OVSDB connection transaction (integer value)
#ovsdb_connection_timeout = 180

# Max interval, in seconds ,between each retry to get the OVN NB and SB IDLs
# (integer value)
#ovsdb_retry_max_interval = 180

# The probe interval for the OVSDB session, in milliseconds. If this is zero,
# it disables the connection keepalive feature. If non-zero the value will be
# forced to at least 1000 milliseconds. Defaults to 60 seconds. (integer value)
# Minimum value: 0
#ovsdb_probe_interval = 60000

# The synchronization mode of OVN_Northbound OVSDB with Neutron DB.
# off - synchronization is off
# log - during neutron-server startup, check to see if OVN is in sync with the
# Neutron database.  Log warnings for any inconsistencies found so that an
# admin can investigate
# repair - during neutron-server startup, automatically create resources found
# in Neutron but not in OVN. Also remove resources from OVN that are no longer
# in Neutron.migrate - This mode is to OVS to OVN migration. It will sync the
# DB just like repair mode but it will additionally fix the Neutron DB resource
# from OVS to OVN. (string value)
# Possible values:
# off - <No description provided>
# log - <No description provided>
# repair - <No description provided>
# migrate - <No description provided>
#neutron_sync_mode = log

# The OVN L3 Scheduler type used to schedule router gateway ports on
# hypervisors/chassis.
# leastloaded - chassis with fewest gateway ports selected
# chance - chassis randomly selected (string value)
# Possible values:
# leastloaded - <No description provided>
# chance - <No description provided>
#ovn_l3_scheduler = leastloaded

# Enable distributed floating IP support.
# If True, the NAT action for floating IPs will be done locally and not in the
# centralized gateway. This saves the path to the external network. This
# requires the user to configure the physical network map (i.e. ovn-bridge-
# mappings) on each compute node. (boolean value)
#enable_distributed_floating_ip = false

# The directory in which vhost virtio sockets are created by all the vswitch
# daemons (string value)
#vhost_sock_dir = /var/run/openvswitch

# Default lease time (in seconds) to use with OVN's native DHCP service.
# (integer value)
#dhcp_default_lease_time = 43200

# The log level used for OVSDB (string value)
# Possible values:
# CRITICAL - <No description provided>
# ERROR - <No description provided>
# WARNING - <No description provided>
# INFO - <No description provided>
# DEBUG - <No description provided>
#ovsdb_log_level = INFO

# Whether to use metadata service. (boolean value)
#ovn_metadata_enabled = false

# Comma-separated list of the DNS servers which will be used as forwarders if a
# subnet's dns_nameservers field is empty. If both subnet's dns_nameservers and
# this option are empty, then the DNS resolvers on the host running the neutron
# server will be used. (list value)
#dns_servers =

# Dictionary of global DHCPv4 options which will be automatically set on each
# subnet upon creation and on all existing subnets when Neutron starts.
# An empty value for a DHCP option will cause that option to be unset globally.
# EXAMPLES:
# - ntp_server:1.2.3.4,wpad:1.2.3.5 - Set ntp_server and wpad
# - ntp_server:,wpad:1.2.3.5 - Unset ntp_server and set wpad
# See the ovn-nb(5) man page for available options. (dict value)
#ovn_dhcp4_global_options =

# Dictionary of global DHCPv6 options which will be automatically set on each
# subnet upon creation and on all existing subnets when Neutron starts.
# An empty value for a DHCPv6 option will cause that option to be unset
# globally.
# See the ovn-nb(5) man page for available options. (dict value)
#ovn_dhcp6_global_options =

# Configure OVN to emit "need to frag" packets in case of MTU mismatches.
# Before enabling this option make sure that it is supported by the host kernel
# (version >= 5.2) or by checking the output of the following command:
# ovs-appctl -t ovs-vswitchd dpif/show-dp-features br-int | grep "Check pkt
# length action". (boolean value)
#ovn_emit_need_to_frag = false

# Disable OVN's built-in DHCP for baremetal ports (VNIC type "baremetal"). This
# allows operators to plug their own DHCP server of choice for PXE booting
# baremetal nodes. OVN 23.06.0 and newer also supports baremetal ``PXE`` based
# provisioning over IPv6. If an older version of OVN is used for baremetal
# provisioning over IPv6 this option should be set to "True" and neutron-dhcp-
# agent should be used instead. Defaults to "False". (boolean value)
#disable_ovn_dhcp_for_baremetal_ports = false

# DEPRECATED: If OVN older than 21.06 is used together with Neutron, this
# option should be set to ``False`` in order to disable the ``stateful-
# security-group`` API extension as ``allow-stateless`` keyword is only
# supported by OVN >= 21.06. (boolean value)
# This option is deprecated for removal since 2023.1.
# Its value may be silently ignored in the future.
#allow_stateless_action_supported = true

# If enabled it will allow localnet ports to learn MAC addresses and store them
# in FDB SB table. This avoids flooding for traffic towards unknown IPs when
# port security is disabled. It requires OVN 22.09 or newer. (boolean value)
#localnet_learn_fdb = false

# The number of seconds to keep FDB entries in the OVN DB. The value defaults
# to 0, which means disabled. This is supported by OVN >= 23.09. (integer
# value)
# Minimum value: 0
#fdb_age_threshold = 0

# The number of seconds to keep MAC_Binding entries in the OVN DB. 0 to disable
# aging. (integer value)
# Minimum value: 0
#mac_binding_age_threshold = 0


[ovn_nb_global]

#
# From neutron.ml2.ovn
#

# If set to False, ARP/ND reply flows for logical switch ports will be
# installed only if the port is UP, i.e. claimed by a Chassis. If set to True,
# these flows are installed regardless of the status of the port, which can
# result in a situation that an ARP request to an IP is resolved even before
# the relevant VM/container is running. For environments where this is not an
# issue, setting it to True can reduce the load and latency of the control
# plane. The default value is False. (boolean value)
#ignore_lsp_down = false

# FDB aging bulk removal limit. This limits how many rows can expire in a
# single transaction. Default is 0, which is unlimited. When the limit is
# reached, the next batch removal is delayed by 5 seconds. This is supported by
# OVN >= 23.09. (integer value)
# Minimum value: 0
#fdb_removal_limit = 0

# MAC binding aging bulk removal limit. This limits how many entries can expire
# in a single transaction. The default is 0 which is unlimited. When the limit
# is reached, the next batch removal is delayed by 5 seconds. (integer value)
# Minimum value: 0
#mac_binding_removal_limit = 0


[ovs]

#
# From neutron.ml2.ovn
#

# Timeout in seconds for OVSDB commands. If the timeout expires, OVSDB commands
# will fail with ALARMCLOCK error. (integer value)
#ovsdb_timeout = 10

# The maximum number of MAC addresses to learn on a bridge managed by the
# Neutron OVS agent. Values outside a reasonable range (10 to 1,000,000) might
# be overridden by Open vSwitch according to the documentation. (integer value)
#bridge_mac_table_size = 50000

# Enable IGMP snooping for integration bridge. If this option is set to True,
# support for Internet Group Management Protocol (IGMP) is enabled in
# integration bridge. (boolean value)
#igmp_snooping_enable = false

# Multicast packets (except reports) are unconditionally forwarded to the ports
# bridging a logical network to a physical network. (boolean value)
#igmp_flood = false

# Multicast reports are unconditionally forwarded to the ports bridging a
# logical network to a physical network. (boolean value)
#igmp_flood_reports = true

# This option enables or disables flooding of unregistered multicast packets to
# all ports. If False, The switch will send unregistered multicast packets only
# to ports connected to multicast routers. (boolean value)
#igmp_flood_unregistered = false


[ovs_driver]

#
# From neutron.ml2
#

# Comma-separated list of VNIC types for which support is administratively
# prohibited by the mechanism driver. Please note that the supported vnic_types
# depend on your network interface card, on the kernel version of your
# operating system, and on other factors, like OVS version. In case of ovs
# mechanism driver the valid vnic types are normal and direct. Note that direct
# is supported only from kernel 4.8, and from ovs 2.8.0. Bind DIRECT (SR-IOV)
# port allows to offload the OVS flows using tc to the SR-IOV NIC. This allows
# to support hardware offload via tc and that allows us to manage the VF by
# OpenFlow control plane using representor net-device. (list value)
#vnic_type_prohibit_list =


[securitygroup]

#
# From neutron.ml2
#

# Driver for security groups firewall in the L2 agent (string value)
#firewall_driver = <None>

# Controls whether the neutron security group API is enabled in the server. It
# should be false when using no security groups or using the Nova security
# group API. (boolean value)
#enable_security_group = true

# Use IPsets to speed-up the iptables based security groups. Enabling IPset
# support requires that ipset is installed on the L2 agent node. (boolean
# value)
#enable_ipset = true

# Comma-separated list of ethertypes to be permitted, in hexadecimal (starting
# with "0x"). For example, "0x4008" to permit InfiniBand. (list value)
#permitted_ethertypes =


[sriov_driver]

#
# From neutron.ml2
#

# Comma-separated list of VNIC types for which support is administratively
# prohibited by the mechanism driver. Please note that the supported vnic_types
# depend on your network interface card, on the kernel version of your
# operating system, and on other factors. In the case of SRIOV mechanism
# drivers the valid VNIC types are direct, macvtap and direct-physical. (list
# value)
#vnic_type_prohibit_list =
