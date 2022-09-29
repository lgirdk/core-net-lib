/*
* Copyright 2020 Comcast Cable Communications Management, LLC
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* SPDX-License-Identifier: Apache-2.0
*/
#ifndef _LIBNET_H
#define _LIBNET_H

#include <net/if.h>

#define IP_ARG_SIZE 128

/*
 *        Neighbour Cache Entry States.
 */
#define NEIGH_STATE_INCOMPLETE   0x01
#define NEIGH_STATE_REACHABLE    0x02
#define NEIGH_STATE_STALE        0x04
#define NEIGH_STATE_DELAY        0x08
#define NEIGH_STATE_PROBE        0x10
#define NEIGH_STATE_FAILED       0x20
#define NEIGH_STATE_NOARP        0x40
#define NEIGH_STATE_PERMANENT    0x80
#define NEIGH_STATE_NONE         0x00
#endif

#define CNL_LOG_INFO(format, ...)     \
                              fprintf (stderr, "%s: "format, __FUNCTION__, ##__VA_ARGS__)
#define CNL_LOG_ERROR(format, ...)    \
                              fprintf (stderr, "%s: "format, __FUNCTION__, ##__VA_ARGS__)
#define CNL_LOG_NOTICE(format, ...)   \
                              fprintf (stderr, "%s: "format, __FUNCTION__, ##__VA_ARGS__)
#define CNL_LOG_WARNING(format, ...)  \
                              fprintf (stderr, "%s: "format, __FUNCTION__, ##__VA_ARGS__)

typedef enum {
        CNL_STATUS_SUCCESS = 0,
        CNL_STATUS_FAILURE = -1
} libnet_status;

typedef enum {
        STP_DISABLED = 0,
        STP_LISTENING = 1,
        STP_LEARNING = 2,
        STP_FORWARDING = 3,
        STP_BLOCKING = 4,
} bridge_stp_state;

struct bridge_info {
#define MAX_SLAVE_COUNT 8
        int slave_count; /* Number of availble slave count */
        struct nl_cache *link_cache;
        char *slave_name[MAX_SLAVE_COUNT]; /* slave names are stored in each index */
};

/*
 *        bridge stp states
 */
struct _neighbour_info {
        int state; /* neighbour table reachable state */
        char *local; /* ip address */
        char *mac; /* mac address */
};

struct neighbour_info {
#define MAX_NEIGH_COUNT 32
        int neigh_count;
        struct _neighbour_info neigh_arr[MAX_NEIGH_COUNT];
};

typedef enum _cnl_ifstats_mask {
        IFSTAT_RXTX_PACKET  = 0x01,
        IFSTAT_RXTX_BYTES   = 0x02,
        IFSTAT_RXTX_ERRORS  = 0x04,
        IFSTAT_RXTX_DROPPED = 0x08,
        IFSTAT_RXTX_ALL     = 0x0F
} cnl_ifstats_mask;

typedef struct _cnl_iface_stats {
        uint64_t rx_packet; /* Packets received */
        uint64_t tx_packet; /* Packets sent */
        uint64_t rx_bytes; /* Bytes received */
        uint64_t tx_bytes; /* Bytes sent */
        uint64_t rx_errors; /* Receive errors */
        uint64_t tx_errors; /* Send errors */
        uint64_t rx_dropped; /* Received packets dropped */
        uint64_t tx_dropped; /* Packets dropped during transmit */
        // Add as required
} cnl_iface_stats;

/**
 * file_write
 * @file_name: File name to write
 * @buf: write buffer
 * @count: No of bytes to be written
 *
 * Write buffer to a file
 */
libnet_status file_write(const char *file_name ,const char *buf, size_t count);

#define write_kernel_param(KERNEL_PARAM, VAL) file_write(KERNEL_PARAM , VAL, strlen(VAL)+1)

/**
 * file_read
 * @file_name: File name to read
 * @buf: read buffer
 * @count: No of bytes to be read
 *
 * Read from file to buffer
 */
libnet_status file_read(const char *file_name, char *buf, size_t count);

/**
 * vlan_create
 * @if_name: Name of the interface
 * @vid: vlan id
 *
 * Add vlan interface
 */
libnet_status vlan_create(const char *if_name, int vid);

/**
 * vlan_delete
 * @vlan_name: Name of the vlan interface
 *
 * Remove vlan interface
 */
libnet_status vlan_delete(const char* vlan_name);

/**
 * bridge_create
 * @bridge_name: Name of the bridge interface
 *
 * Add bridge interface
 */
libnet_status bridge_create(const char* bridge_name);

/**
 * bridge_delete
 * @bridge_name: Name of the bridge interface
 *
 * Remove bridge interface
 */
libnet_status bridge_delete(const char* bridge_name);

/**
 * bridge_set_stp
 * @bridge_name: Name of the bridge
 * @val: To enable STP or not ("off" if disabled, "on" if enabled)
 *
 * Enable/Disable spanning tree protocol for bridge interface
 */
libnet_status bridge_set_stp(const char *bridge_name, char *val);

/**
 * interface_add_to_bridge
 * @bridge_name: Name of the bridge
 * @if_name: slave interface name
 *
 * Add interface to bridge
 */
libnet_status interface_add_to_bridge(const char* bridge_name, const char* if_name);

/**
 * interface_remove_from_bridge
 * @if_name: slave interface name
 *
 * Remove interface from bridge
 */
libnet_status interface_remove_from_bridge (const char *if_name);

/**
 * bridge_get_info
 * @bridge_name: bridge interface name
 * @bridge: bridge structure to fill
 * Get bridge details
 */
libnet_status bridge_get_info(char *bridge_name, struct bridge_info *bridge);

/**
 * bridge_free_info
 * @bridge: bridge_info memory to free
 * Free bridge_info members
 */
void bridge_free_info(struct bridge_info *bridge);

/**
 * interface_up
 * @if_name: interface name
 *
 * Set interface state as UP
 */
libnet_status interface_up(char *if_name);

/**
 * interface_down
 * @if_name: interface name
 *
 * Set interface state as DOWN
 */
libnet_status interface_down(char *if_name);

/**
 * interface_set_mtu
 * @if_name: interface name
 * @val: new mtu value
 *
 * Set mtu for an interface
 */
libnet_status interface_set_mtu(const char *if_name, char *val);

/**
 * interface_set_flags
 * @if_name: interface name
 * @flags: netdevice flags
 *
 * Set interface flags
 */
libnet_status interface_set_flags(char *if_name, unsigned int flags);

/**
 * interface_set_allmulticast
 * @if_name: interface name
 *
 * Set allmulticast for an interface
 */
#define interface_set_allmulticast(if_name) interface_set_flags(if_name, IFF_ALLMULTI)

/**
 * interface_get_mac
 * @if_name: interface name
 * @mac: read buffer for mac
 * @size: mac buffer size
 *
 * Get mac address of an interface
 */
libnet_status interface_get_mac(const char *if_name, char *mac, size_t size);

/**
 * interface_set_mac
 * @if_name: interface name
 * @mac: write buffer for mac
 *
 * Set mac address of an interface
 */
libnet_status interface_set_mac(const char *if_name, char *mac);

/**
 * interface_get_ip
 * @if_name: interface name
 *
 * Get the ip address of an interface
 */
libnet_status interface_exist(const char *iface_name);

/**
 * interface_set_netmask
 * @if_name: interface name
 * @netmask: netmask
 * Set the netmask address of an interface
 */
libnet_status interface_set_netmask(const char* if_name, const char *netmask);

/**
 * interface_get_ip
 * @if_name: interface name
 *
 * Get the ip address of an interface
 */
char* interface_get_ip(const char* if_name);

/**
 * interface_rename
 * @if_name: interface name
 * @new_name: new interface name
 *
 * Rename interface
 * This operation is not recommended if the device is running or
 * has some addresses already configured.
 */
libnet_status interface_rename(char *if_name, char *new_name);

/**
 * interface_delete
 * @name: interface name
 *
 * Delete an interface
 */
libnet_status interface_delete(char *name);

/**
 * interface_get_stats
 * @ifStatsMask: Get the interface needed information by bitmask
 * @if_name: Name of the interface for information
 * @stats: Structure for interface stats
 *
 * Added for getting interface statistics
 */
libnet_status interface_get_stats(cnl_ifstats_mask ifstats_mask, const char* if_name,
                                  cnl_iface_stats *stats);

/**
 * addr_add
 * @args: ip address, [netmask, broadcast & family]
 *
 * Configure interface ip configurations
 */
libnet_status addr_add(char *args);

#define addr_add_va_arg(FMT, ...) ({                    \
                        int ret;                        \
                        char buf[IP_ARG_SIZE];          \
                        sprintf(buf,FMT, __VA_ARGS__);  \
                        ret = addr_add(buf);            \
                        ret;                            \
                })

#define interface_set_ip(if_name, address)              \
                addr_add_va_arg("dev %s %s", if_name, address)

/**
 * addr_delete
 * @args: ip address, [netmask, broadcast & family]
 *
 * Delete ip address of an interface
 */
libnet_status addr_delete(char *args);

#define addr_delete_va_arg(FMT, ...) ({                 \
                        int ret;                        \
                        char buf[IP_ARG_SIZE];          \
                        sprintf(buf,FMT, __VA_ARGS__);  \
                        ret = addr_delete(buf);         \
                        ret;                            \
                })

/**
 * addr_derive_broadcast
 * @ip: IP address of the interface
 * @prefix_len: address bits (excluding broadcast bits)
 * @bcast: broadcast address
 * @size: broadcast buffer size
 *
 * Derive broadcast address using ip & prefix values
 */
libnet_status addr_derive_broadcast(char *ip, unsigned int prefix_len, char *bcast, int size);

/**
 * route_add
 * @args: routing configuration [default, device, table, via]
 *
 * Add routing table entry
 */
libnet_status route_add(char *args);

#define route_add_va_arg(FMT, ...) ({                   \
                        int ret;                        \
                        char buf[IP_ARG_SIZE];          \
                        sprintf(buf,FMT, __VA_ARGS__);  \
                        ret = route_add(buf);           \
                        ret;                            \
                })

/**
 * route_delete
 * @args: routing configuration [default, device, table, via]
 *
 * Delete routing table entry
 */
libnet_status route_delete(char *args);

#define route_delete_va_arg(FMT, ...) ({                \
                        int ret;                        \
                        char buf[IP_ARG_SIZE];          \
                        sprintf(buf,FMT, __VA_ARGS__);  \
                        ret = route_delete(buf);        \
                        ret;                            \
                })

/**
 * rule_add
 * @args: policy routing configuration [input interface, output interface, table]
 *
 * Add policy routing table entry
 */
libnet_status rule_add(char *arg);

#define rule_add_va_arg(FMT, ...) ({                    \
                        int ret;                        \
                        char buf[IP_ARG_SIZE];          \
                        sprintf(buf,FMT, __VA_ARGS__);  \
                        ret = rule_add(buf);            \
                        ret;                            \
                })

/**
 * rule_delete
 * @args: policy routing configuration [input interface, output interface, table]
 *
 * Remove policy routing table entry
 */
libnet_status rule_delete(char *arg);

#define rule_delete_va_arg(FMT, ...) ({                 \
                        int ret;                        \
                        char buf[IP_ARG_SIZE];          \
                        sprintf(buf,FMT, __VA_ARGS__);  \
                        ret = rule_delete(buf);         \
                        ret;                            \
                })

/**
 * tunnel_add_ip4ip6
 *  @tunnel_name: Tunnel interface name
 *  @dev_name: Device name
 *  @local_ip6: Local ipv6 address
 *  @remote_ip6: Remote ipv6 address
 *  @encaplimit: Encapsulation max size
 * Add ip4ip6 tunnel interface
 */
libnet_status tunnel_add_ip4ip6(const char *tunnel_name, const char *dev_name,
                                const char *local_ip6, const char *remote_ip6,
                                const char *encaplimit);

#define tunnel_delete_ip4ip6(tunnel_name) interface_delete(tunnel_name)

/**
 * neighbour_delete
 * @dev: interface name
 * @ip: ip address
 *
 * Delete an entry in neighbour table
 */
libnet_status neighbour_delete(char *dev, char *ip);

/**
 * neighbour_get_list - lists all entries except for NEIGH_STATE_NONE,
 * NEIGH_STATE_NOARP and NEIGH_STATE_PERMANENT.
 * @arr: array to fill neighbour table
 * Get bridge details
 */
libnet_status neighbour_get_list(struct neighbour_info *arr);
