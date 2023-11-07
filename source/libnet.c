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

#include <unistd.h>
#include <sys/ioctl.h>
#include <net/if_arp.h>
#include <arpa/inet.h>
#include <errno.h>

#include <netlink/route/neighbour.h>
#include <netlink/route/addr.h>
#include <netlink/route/rule.h>
#include <netlink/route/link/vlan.h>
#include <netlink/route/link/ip6tnl.h>

#include "safec_lib_common.h"

#include "libnet_util.h"
#include "libnet.h"

/**
 * file_write
 * @file_name: File name to write
 * @buf: write buffer
 * @count: No of bytes to be written
 *
 * Write buffer to a file
 */
libnet_status file_write(const char *file_name ,const char *buf, size_t count)
{
        FILE *fp = NULL;
        libnet_status err = CNL_STATUS_FAILURE;

        fp = fopen(file_name, "w");
        if(NULL == fp)
                return err;
        if (0 <= fwrite(buf, sizeof(char), count, fp))
                err = CNL_STATUS_SUCCESS;
        fclose(fp);
        return err;
}

/**
 * file_read
 * @file_name: File name to read
 * @buf: read buffer
 * @count: No of bytes to be read
 *
 * Read from file to buffer
 */
libnet_status file_read(const char *file_name, char *buf, size_t count)
{
        FILE *fp;
        libnet_status err = CNL_STATUS_FAILURE;
        errno_t rc;

        rc = memset_s(buf, count, 0, count);
        ERR_CHK(rc);

        fp = fopen(file_name, "r");
        if(NULL == fp)
                return err;
        if(0 <= fread(buf, sizeof(char), count, fp))
                err = CNL_STATUS_SUCCESS;
        fclose(fp);

        return err;
}

/**
 * vlan_create
 * @if_name: Name of the interface
 * @vid: vlan id
 *
 * Add vlan interface
 */
libnet_status vlan_create(const char *if_name, int vid)
{
        struct rtnl_link *link;
        struct nl_cache *link_cache;
        struct nl_sock *sk;
        int master_index;
        libnet_status err = CNL_STATUS_FAILURE;
        char vlan_if_name[32] = {0};
        errno_t rc;

        rc = sprintf_s(vlan_if_name, sizeof(vlan_if_name), "%s.%d", if_name,vid);
        ERR_CHK(rc);
        if (rc < EOK)
                return CNL_STATUS_FAILURE;

        sk = libnet_alloc_socket();
        if (sk == NULL) {
                CNL_LOG_ERROR("Unable to allocate memory for socket\n");
                return err;
        }

        if (libnet_connect(sk, NETLINK_ROUTE) < 0) {
                CNL_LOG_ERROR("Unable to connect socket\n");
                goto FREE_SOCKET;
        }

        if (rtnl_link_alloc_cache(sk, AF_UNSPEC, &link_cache) < 0) {
                CNL_LOG_ERROR("Unable to allocate cache\n");
                goto FREE_SOCKET;
        }

        if (!(master_index = rtnl_link_name2i(link_cache, if_name))) {
                CNL_LOG_ERROR("Unable to lookup %s", if_name);
                goto FREE_CACHE;
        }

        if ((link = rtnl_link_vlan_alloc()) == NULL) {
                CNL_LOG_ERROR("Unable to allocate memory for vlan\n");
                goto FREE_CACHE;
        }
        rtnl_link_set_link(link, master_index);
        rtnl_link_set_name(link, vlan_if_name);

        if (rtnl_link_vlan_set_id(link, vid) < 0) {
                CNL_LOG_ERROR("Unable to set vlan id\n");
                goto FREE_CACHE;
        }

        if (rtnl_link_add(sk, link, NLM_F_CREATE | NLM_F_EXCL ) < 0) {
                CNL_LOG_ERROR("Unable to add link\n");
                goto FREE_VLAN;
        }
        err = CNL_STATUS_SUCCESS;
FREE_VLAN:
        rtnl_link_put(link);
FREE_CACHE:
        nl_cache_free(link_cache);
FREE_SOCKET:
        nl_socket_free(sk);

        return err;
}

/**
 * vlan_delete
 * @vlan_name: Name of the vlan interface
 *
 * Remove vlan interface
 */
libnet_status vlan_delete(const char *vlan_name)
{
        struct rtnl_link *link;
        struct nl_sock *sk;
        libnet_status err = CNL_STATUS_FAILURE;

        sk = libnet_alloc_socket();
        if (sk == NULL) {
                CNL_LOG_ERROR("Unable to allocate memory for socket\n");
                return err;
        }

        if (libnet_connect(sk, NETLINK_ROUTE) < 0) {
                CNL_LOG_ERROR("Unable to connect socket\n");
                goto FREE_SOCKET;
        }

        if ((link = rtnl_link_alloc()) == NULL) {
                CNL_LOG_ERROR("Unable to allocate memory for link\n");
                goto FREE_SOCKET;
        }
        rtnl_link_set_name(link, vlan_name);

        if (rtnl_link_delete(sk, link) < 0) {
                CNL_LOG_ERROR("Unable to delete vlan\n");
                goto FREE_LINK;
        }
        err = CNL_STATUS_SUCCESS;
FREE_LINK:
        rtnl_link_put(link);
FREE_SOCKET:
        nl_socket_free(sk);

        return err;
}

/**
 * bridge_create
 * @bridge_name: Name of the bridge interface
 *
 * Add bridge interface
 */
libnet_status bridge_create(const char* bridge_name)
{
        struct rtnl_link *link;
        struct nl_sock *sk;
        libnet_status err = CNL_STATUS_FAILURE;

        sk = libnet_alloc_socket();
        if (sk == NULL) {
                CNL_LOG_ERROR("Unable to allocate memory for socket\n");
                return err;
        }

        if (libnet_connect(sk, NETLINK_ROUTE) < 0) {
                CNL_LOG_ERROR("Unable to connect socket\n");
                goto FREE_SOCKET;
        }

        if ((link = rtnl_link_alloc()) == NULL) {
                CNL_LOG_ERROR("Unable to allocate memory for link\n");
                goto FREE_SOCKET;
        }
        if (rtnl_link_set_type(link, "bridge") < 0) {
                CNL_LOG_ERROR("Unable to set link type\n");
                goto FREE_LINK;
        }
        rtnl_link_set_name(link, bridge_name);

        if (rtnl_link_add(sk, link, NLM_F_CREATE | NLM_F_EXCL) < 0) {
                CNL_LOG_ERROR("Unable to allocate bridge\n");
                goto FREE_LINK;
        }
        err = CNL_STATUS_SUCCESS;
FREE_LINK:
        rtnl_link_put(link);
FREE_SOCKET:
        nl_socket_free(sk);

        return err;
}

/**
 * bridge_delete
 * @bridge_name: Name of the bridge interface
 *
 * Remove bridge interface
 */
libnet_status bridge_delete(const char* bridge_name)
{
        struct rtnl_link *link;
        struct nl_sock *sk;
        libnet_status err = CNL_STATUS_FAILURE;

        sk = libnet_alloc_socket();
        if (sk == NULL) {
                CNL_LOG_ERROR("Unable to allocate memory for socket\n");
                return err;
        }

        if (libnet_connect(sk, NETLINK_ROUTE) < 0) {
                CNL_LOG_ERROR("Unable to connect socket\n");
                goto FREE_SOCKET;
        }

        if ((link = rtnl_link_alloc()) == NULL ) {
                CNL_LOG_ERROR("Unable to allocate memory for link\n");
                goto FREE_SOCKET;
        }
        rtnl_link_set_name(link, bridge_name);

        if (rtnl_link_delete(sk, link) < 0) {
                CNL_LOG_ERROR("Unable to delete bridge\n");
                goto FREE_LINK;
        }
        err = CNL_STATUS_SUCCESS;
FREE_LINK:
        rtnl_link_put(link);
FREE_SOCKET:
        nl_socket_free(sk);

        return err;
}

/**
 * interface_add_to_bridge
 * @bridge_name: Name of the bridge
 * @if_name: slave interface name
 *
 * Add interface to bridge
 */
libnet_status interface_add_to_bridge(const char* bridge_name, const char* if_name)
{
        struct nl_sock *sk;
        struct nl_cache *link_cache;
        struct rtnl_link *link;
        struct rtnl_link *ltap;
        libnet_status err = CNL_STATUS_FAILURE;

        sk = libnet_alloc_socket();
        if (sk == NULL) {
                CNL_LOG_ERROR("Unable to allocate memory for socket\n");
                return err;
        }

        if (libnet_connect(sk, NETLINK_ROUTE) < 0) {
                CNL_LOG_ERROR("Unable to connect socket\n");
                goto FREE_SOCKET;
        }

        if (rtnl_link_alloc_cache(sk, AF_UNSPEC, &link_cache) < 0) {
                CNL_LOG_ERROR("Unable to allocate cache\n");
                goto FREE_SOCKET;
        }

        link = rtnl_link_get_by_name(link_cache, bridge_name);
        if (!link) {
                CNL_LOG_ERROR("Unable to find the bridge %s)\n", bridge_name);
                goto FREE_CACHE;
        }

        ltap = rtnl_link_get_by_name(link_cache, if_name);
        if (!ltap) {
                CNL_LOG_ERROR("Unable to find the interface %s\n", if_name);
                goto FREE_LINK;
        }

        if (rtnl_link_enslave(sk, link, ltap) < 0) {
                CNL_LOG_ERROR("Unable to enslave interface to bridge\n");
                goto FREE_LINK2;
        }
        err = CNL_STATUS_SUCCESS;
FREE_LINK2:
        rtnl_link_put(ltap);
FREE_LINK:
        rtnl_link_put(link);
FREE_CACHE:
        nl_cache_free(link_cache);
FREE_SOCKET:
        nl_socket_free(sk);

        return err;
}

/**
 * interface_remove_from_bridge
 * @if_name: slave interface name
 *
 * Remove interface from bridge
 */
libnet_status interface_remove_from_bridge (const char *if_name)
{
        struct nl_sock *sk;
        struct nl_cache *link_cache;
        struct rtnl_link *ltap;
        libnet_status err = CNL_STATUS_FAILURE;

        sk = libnet_alloc_socket();
        if (sk == NULL) {
                CNL_LOG_ERROR("Unable to allocate memory for socket\n");
                return err;
        }

        if (libnet_connect(sk, NETLINK_ROUTE) < 0) {
                CNL_LOG_ERROR("Unable to connect socket\n");
                goto FREE_SOCKET;
        }

        if (rtnl_link_alloc_cache(sk, AF_UNSPEC, &link_cache) < 0) {
                CNL_LOG_ERROR("Unable to allocate cache\n");
                goto FREE_SOCKET;
        }

        ltap = rtnl_link_get_by_name(link_cache, if_name);
        if (!ltap) {
                CNL_LOG_ERROR("Unable to find the interface %s\n", if_name);
                goto FREE_CACHE;
        }

        if (rtnl_link_release(sk, ltap) < 0) {
                CNL_LOG_ERROR("Unable to release interface from bridge\n");
                goto FREE_LINK;
        }
        err = CNL_STATUS_SUCCESS;
FREE_LINK:
        rtnl_link_put(ltap);
FREE_CACHE:
        nl_cache_free(link_cache);
FREE_SOCKET:
        nl_socket_free(sk);

        return err;
}

/**
 * bridge_set_stp
 * @bridge_name: Name of the bridge
 * @val: To enable STP or not ("off" if disabled, "on" if enabled)
 *
 * Enable/Disable spanning tree protocol for bridge interface
 */
libnet_status bridge_set_stp(const char *bridge_name, char *val)
{
        char file_name[64] = {0};
        char buf[2] = {0};
        int len = (val != NULL ? strlen(val) : 0);
        int off = 0, on = 0;
        int stp_state;
        errno_t rc;

        if (len == 3)
        {
                off = (strncmp(val, "off", 3) == 0 ? 1 : 0);
        }
        else if (len == 2)
        {
                on = (strncmp(val, "on", 2) == 0 ? 1 : 0);
        }

        if ((off == 0 && on == 0) ||
            (off == 1 && on == 1))
        {
                CNL_LOG_ERROR("func %s: val must be on or off\n", __func__);
                return CNL_STATUS_FAILURE;
        }

        stp_state = CNL_STATUS_FAILURE;

        if (off == 1)
        {
                // Off
                stp_state = (int) STP_DISABLED;
        }
        else
        {
                // On
                stp_state = (int) STP_LISTENING;
        }

        rc = sprintf_s(file_name, sizeof(file_name), "/sys/class/net/%s/bridge/stp_state",
                       bridge_name);
        ERR_CHK(rc);
        if (rc < EOK)
                return CNL_STATUS_FAILURE;

        if (-1 == access(file_name, F_OK | W_OK))
        {
                CNL_LOG_ERROR("func %s: file_name %s does not exist or is not writeable\n",
                              __func__, file_name);
                return CNL_STATUS_FAILURE;
        }

        buf[0] = (char) stp_state + '0';
        return file_write(file_name, buf, strlen(buf) + 1);
}

/**
 * interface_up
 * @if_name: interface name
 *
 * Set interface state as UP
 */
libnet_status interface_up(char *if_name)
{
        struct nl_sock *sk;
        struct rtnl_link *link;
        struct rtnl_link *change;
        struct nl_cache *cache;
        libnet_status err = CNL_STATUS_FAILURE;

        sk = libnet_alloc_socket();
        if (sk == NULL) {
                CNL_LOG_ERROR("Unable to allocate memory for socket\n");
                return err;
        }

        if (libnet_connect(sk, NETLINK_ROUTE) < 0) {
                CNL_LOG_ERROR("Unable to connect socket\n");
                goto FREE_SOCKET;
        }

        if (rtnl_link_alloc_cache(sk, AF_UNSPEC, &cache) < 0) {
                CNL_LOG_ERROR("Unable to allocate cache\n");
                goto FREE_SOCKET;
        }

        if (!(link = rtnl_link_get_by_name(cache, if_name))) {
                CNL_LOG_ERROR("Interface not found\n");
                goto FREE_CACHE;
        }

        unsigned int flags = rtnl_link_get_flags(link);
        if (flags & IFF_UP) {
                err = CNL_STATUS_SUCCESS;
                goto FREE_LINK;
        }

        change = rtnl_link_alloc();

        flags |= IFF_UP;

        rtnl_link_set_flags(change, flags);
        if (rtnl_link_change(sk, link, change, 0) < 0) {
                CNL_LOG_ERROR("Unable to activate\n");
                goto FREE_LINK2;
        }

        err = CNL_STATUS_SUCCESS;
FREE_LINK2:
        rtnl_link_put(change);
FREE_LINK:
        rtnl_link_put(link);
FREE_CACHE:
        nl_cache_free(cache);
FREE_SOCKET:
        nl_socket_free(sk);

        return err;
}

/**
 * interface_down
 * @if_name: interface name
 *
 * Set interface state as DOWN
 */
libnet_status interface_down(char *if_name)
{
        struct nl_sock *sk;
        struct rtnl_link *link;
        struct rtnl_link *change;
        struct nl_cache *cache;
        libnet_status err = CNL_STATUS_FAILURE;

        sk = libnet_alloc_socket();
        if (sk == NULL) {
                CNL_LOG_ERROR("Unable to allocate memory for socket\n");
                return err;
        }

        if (libnet_connect(sk, NETLINK_ROUTE) < 0) {
                CNL_LOG_ERROR("Unable to connect socket\n");
                goto FREE_SOCKET;
        }

        if (rtnl_link_alloc_cache(sk, AF_UNSPEC, &cache) < 0) {
                CNL_LOG_ERROR("Unable to allocate cache\n");
                goto FREE_SOCKET;
        }

        if (!(link = rtnl_link_get_by_name(cache, if_name))) {
                CNL_LOG_ERROR("Interface not found\n");
                goto FREE_CACHE;
        }

        unsigned int flags = rtnl_link_get_flags(link);
        if (!(flags & IFF_UP)) {
                err = CNL_STATUS_SUCCESS;
                goto FREE_LINK;
        }

        change = rtnl_link_alloc();
        rtnl_link_unset_flags(change, IFF_UP);

        if (rtnl_link_change(sk, link, change, 0) < 0) {
                CNL_LOG_ERROR("Unable to deactivate\n");
                goto FREE_LINK2;
        }

        err = CNL_STATUS_SUCCESS;
FREE_LINK2:
        rtnl_link_put(change);
FREE_LINK:
        rtnl_link_put(link);
FREE_CACHE:
        nl_cache_free(cache);
FREE_SOCKET:
        nl_socket_free(sk);
        return err;
}

/**
 * interface_exist
 * @if_name: interface name
 *
 * Check the interface existence
 */
int interface_exist(const char *if_name)
{
        char file_name[64] = {0};
        errno_t rc;

        rc = sprintf_s(file_name, sizeof(file_name), "/sys/class/net/%s", if_name);
        ERR_CHK(rc);
        if (rc < EOK)
                return CNL_STATUS_FAILURE;

        if (0 == access(file_name, F_OK))
                return CNL_STATUS_SUCCESS;
        return CNL_STATUS_FAILURE;
}

/**
 * interface_set_mtu
 * @if_name: interface name
 * @val: new mtu value
 *
 * Set mtu fo an interface
 */
libnet_status interface_set_mtu(const char *if_name, char *val)
{
        char file_name[64] = {0};
        errno_t rc;

        rc = sprintf_s(file_name, sizeof(file_name), "/sys/class/net/%s/mtu", if_name);
        ERR_CHK(rc);
        if (rc < EOK)
                return CNL_STATUS_FAILURE;

        return file_write(file_name, val, strlen(val) + 1);
}

/**
 * interface_get_mac
 * @if_name: interface name
 * @mac: read buffer for mac
 * @size: mac buffer size
 *
 * Get mac address of an interface
 */
libnet_status interface_get_mac(const char *if_name, char *mac, size_t size)
{
        char file_name[64] = {0};
        libnet_status err = CNL_STATUS_FAILURE;
        errno_t rc;

        rc = sprintf_s(file_name, sizeof(file_name), "/sys/class/net/%s/address", if_name);
        ERR_CHK(rc);
        if (rc < EOK)
                return CNL_STATUS_FAILURE;

        err = file_read(file_name, mac, size);
        if (err != CNL_STATUS_FAILURE) {
                while (*mac != '\0')
                        mac++;
                if (*(--mac) == '\n')
                        *mac = '\0';
        }
        return err;
}

/**
 * interface_set_mac
 * @if_name: interface name
 * @mac: write buffer for mac
 *
 * Set mac address of an interface
 */
libnet_status interface_set_mac(const char *if_name, char *mac)
{
        struct ifreq if_req = {0};
        int sock;

        int ret = sscanf(mac, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                         &if_req.ifr_hwaddr.sa_data[0],
                         &if_req.ifr_hwaddr.sa_data[1],
                         &if_req.ifr_hwaddr.sa_data[2],
                         &if_req.ifr_hwaddr.sa_data[3],
                         &if_req.ifr_hwaddr.sa_data[4],
                         &if_req.ifr_hwaddr.sa_data[5]
                );

        if (ret != 6 || strlen(mac) != 17)
        {
                CNL_LOG_ERROR("%s: Input MAC Address must be of format: ab:cd:ef:gh:ij:kl\n",
                              __func__);
                return CNL_STATUS_FAILURE;
        }

        sock = socket(AF_INET, SOCK_DGRAM, 0);

        if (sock < 0)
        {
                CNL_LOG_ERROR("%s: Error creating socket\n", __func__);
                return CNL_STATUS_FAILURE;
        }

        strcpy(if_req.ifr_name, if_name);
        if_req.ifr_hwaddr.sa_family = ARPHRD_ETHER;

        if(ioctl(sock, SIOCSIFHWADDR, &if_req) < 0)
        {
                CNL_LOG_ERROR("%s: failed to set MAC Address\n", __func__);
                close(sock);
                return CNL_STATUS_FAILURE;
        }

        close(sock);
        return CNL_STATUS_SUCCESS;
}

/**
 * interface_get_ip
 * @if_name: interface name
 *
 * Get the ip address of an interface
 */
char* interface_get_ip(const char* if_name)
{
        struct ifreq if_req = {0};
        int sock;

        if ((sock = socket(AF_INET, SOCK_DGRAM, 0) ) < 0) {
                CNL_LOG_ERROR("socket error %s \n", strerror(errno));
                return NULL;
        }
        if_req.ifr_addr.sa_family = AF_INET;
        strncpy(if_req.ifr_name, if_name, IFNAMSIZ-1);
        if ( ioctl(sock, SIOCGIFADDR, &if_req)  < 0 )
        {
                CNL_LOG_ERROR("Failed to get %s IP Address \n",if_name);
                close(sock);
                return NULL;
        }
        close(sock);
        return (inet_ntoa(((struct sockaddr_in *)&if_req.ifr_addr)->sin_addr));
}

/**
 * interface_set_netmask
 * @if_name: interface name
 * @netmask: netmask
 * Set the netmask address of an interface
 */
libnet_status interface_set_netmask(const char* if_name, const char *netmask)
{
        struct ifreq if_req = {0};
        struct sockaddr_in sin;
        int sock;

        sock = socket(AF_INET, SOCK_DGRAM, 0);
        sin.sin_family = AF_INET;

        if (inet_aton(netmask,&sin.sin_addr) != 1)
        {
                CNL_LOG_ERROR("Failed to parse %s netmask address\n", if_name);
                close(sock);
                return CNL_STATUS_FAILURE;
        }

        strncpy(if_req.ifr_name, if_name, IFNAMSIZ-1);
        memcpy(&if_req.ifr_netmask, &sin, sizeof(struct sockaddr));

        if (ioctl(sock, SIOCSIFNETMASK, &if_req) < 0) {
                CNL_LOG_ERROR("Failed to set %s netmask address \n",if_name );
                close(sock);
                return CNL_STATUS_FAILURE;
        }
        close(sock);
        return CNL_STATUS_SUCCESS;
}

/**
 * addr_derive_broadcast
 * @ip: IP address of the interface
 * @prefix_len: address bits (excluding broadcast bits)
 * @bcast: broadcast address
 * @size: broadcast buffer size
 *
 * Derive broadcast address using ip & prefix values
 */
libnet_status addr_derive_broadcast(char *ip, unsigned int prefix_len, char *bcast, int size)
{
        struct in_addr inaddr;

        if (inet_pton(AF_INET, ip, &inaddr) < 1)
                return CNL_STATUS_FAILURE;
        inaddr.s_addr |= htonl(~(~0U << (32U - prefix_len)));
        inet_ntop(AF_INET, &inaddr, bcast, size);
        return CNL_STATUS_SUCCESS;
}

/**
 * addr_add
 * @args: ip address, [netmask, broadcast & family]
 *
 * Configure interface ip configurations
 */
libnet_status addr_add(char *args)
{
        struct nl_sock *sock;
        struct rtnl_addr *addr;
        struct nl_cache *link_cache;
        libnet_status err = CNL_STATUS_FAILURE;

        char *str = strdup(args);
        char *token;

        sock = libnet_alloc_socket();
        if (sock == NULL) {
                CNL_LOG_ERROR("Unable to allocate memory for socket\n");
                return err;
        }

        if (libnet_connect(sock, NETLINK_ROUTE) < 0) {
                CNL_LOG_ERROR("Unable to connect socket\n");
                goto FREE_SOCKET;
        }

        link_cache = libnet_link_alloc_cache(sock);
        if (link_cache == NULL) {
                CNL_LOG_ERROR("Unable to allocate link cache\n");
                goto FREE_SOCKET;
        }

        addr = libnet_addr_alloc();
        if (addr == NULL) {
                CNL_LOG_ERROR("Unable to allocate addr\n");
                goto FREE_CACHE;
        }

        token = strtok(str, " ");
        while( token != NULL) {
                if(0 == strcmp(token, "dev")) {
                        token = strtok(NULL, " ");
                        if (token != NULL) {
                                if (libnet_addr_parse_dev(addr, link_cache, token) != 0) {
                                        CNL_LOG_ERROR("%s: Unable to parse dev field\n", args);
                                        goto FREE_ADDR;
                                }
                        }
                } else if(0 == strcmp(token, "valid_lft")) {
                        token = strtok(NULL, " ");
                        if (token != NULL) {
                                if (libnet_addr_parse_valid(addr, token) != 0) {
                                        CNL_LOG_ERROR("%s: Unable to parse valid_lft field \n", args);
                                        goto FREE_ADDR;
                                }
                        }
                } else if(0 == strcmp(token, "preferred_lft")) {
                        token = strtok(NULL, " ");
                        if (token != NULL) {
                                if (libnet_addr_parse_preferred(addr, token) != 0) {
                                        CNL_LOG_ERROR("%s: Unable to parse preferred_lft field\n", args);
                                        goto FREE_ADDR;
                                }
                        }
                } else if(0 == strcmp(token, "broadcast")) {
                        token = strtok(NULL, " ");
                        if (token != NULL) {
                                if (libnet_addr_parse_broadcast(addr, token) != 0) {
                                        CNL_LOG_ERROR("%s: Unable to parse broadcast field\n", args);
                                        goto FREE_ADDR;
                                }
                        }
                } else if(0 == strcmp(token, "-4") || 0 == strcmp(token, "4") ||
                          0 == strcmp(token, "inet")) {
                        rtnl_addr_set_family(addr, AF_INET);
                } else if(0 == strcmp(token, "-6") || 0 == strcmp(token, "6") ||
                          0 == strcmp(token, "inet6")) {
                        rtnl_addr_set_family(addr, AF_INET6);
                } else {
                        if (libnet_addr_parse_local(addr, token) != 0) {
                                CNL_LOG_ERROR("%s: Unable to parse local field\n",
                                              args);
                                goto FREE_ADDR;
                        }
                }
                token = strtok(NULL, " ");
        }

        if (rtnl_addr_add(sock, addr, NLM_F_EXCL) < 0) {
                CNL_LOG_ERROR("%s: Unable to add addr\n", args);
                goto FREE_ADDR;
        }
        err = CNL_STATUS_SUCCESS;

FREE_ADDR:
        rtnl_addr_put(addr);
FREE_CACHE:
        nl_cache_free(link_cache);
FREE_SOCKET:
        nl_socket_free(sock);

        free(str);
        return err;
}

static void addr_delete_cb(struct nl_object *obj, void *arg)
{
        struct rtnl_addr *addr = (struct rtnl_addr *) obj;
        struct nl_sock *sock = (struct nl_sock *) arg;
        libnet_status err = CNL_STATUS_SUCCESS;

        if (err = rtnl_addr_delete(sock, addr, 0) < 0)
                CNL_LOG_ERROR("Unable to delete addr - Returned err %d\n", err);
}

/**
 * addr_delete
 * @args: ip address, [netmask, broadcast & family]
 *
 * Delete ip address of an interface
 */
libnet_status addr_delete(char *args)
{
        char *str = strdup(args);
        char *token;
        int family = AF_UNSPEC;

        static struct nl_sock *sock;
        struct nl_cache *link_cache;
        struct nl_cache *addr_cache;
        struct rtnl_addr *addr;
        libnet_status err = CNL_STATUS_FAILURE;

        sock = libnet_alloc_socket();
        if (sock == NULL) {
                CNL_LOG_ERROR("Unable to allocate memory for socket\n");
                free(str);
                return err;
        }

        if (libnet_connect(sock, NETLINK_ROUTE)< 0) {
                CNL_LOG_ERROR("Unable to connect socket\n");
                goto FREE_SOCKET;
        }

        link_cache = libnet_link_alloc_cache(sock);
        addr_cache = libnet_addr_alloc_cache(sock);
        addr = libnet_addr_alloc();

        token = strtok(str, " ");
        while( token != NULL) {
                if(0 == strcmp(token, "dev")) {
                        token = strtok(NULL, " ");
                        if (token != NULL) {
                                if (libnet_addr_parse_dev(addr, link_cache, token) != 0) {
                                        CNL_LOG_ERROR("%s: Unable to parse dev field\n", args);
                                        goto FREE_ADDR;
                                }
                        }
                } else if(0 == strcmp(token, "valid_lft")) {
                        token = strtok(NULL, " ");
                        if (token != NULL) {
                                if (libnet_addr_parse_valid(addr, token) != 0) {
                                        CNL_LOG_ERROR("%s: Unable to parse valid_lft field\n", args);
                                        goto FREE_ADDR;
                                }
                        }
                } else if(0 == strcmp(token, "preferred_lft")) {
                        token = strtok(NULL, " ");
                        if (token != NULL) {
                                if (libnet_addr_parse_preferred(addr, token) != 0) {
                                        CNL_LOG_ERROR("%s: Unable to parse preferred_lft field\n", args);
                                        goto FREE_ADDR;
                                }
                        }
                } else if(0 == strcmp(token, "broadcast")) {
                        token = strtok(NULL, " ");
                        if (token != NULL) {
                                if (libnet_addr_parse_broadcast(addr, token) != 0) {
                                        CNL_LOG_ERROR("%s: Unable to parse broadcast field\n", args);
                                        goto FREE_ADDR;
                                }
                        }
                } else if(0 == strcmp(token, "-4") || 0 == strcmp(token, "4") ||
                          0 == strcmp(token, "inet")) {
                        rtnl_addr_set_family(addr, AF_INET);
                } else if(0 == strcmp(token, "-6") || 0 == strcmp(token, "6") ||
                          0 == strcmp(token, "inet6")) {
                        rtnl_addr_set_family(addr, AF_INET6);
                } else {
                        if (libnet_addr_parse_local(addr, token) != 0) {
                                CNL_LOG_ERROR("%s: Unable to parse local field\n", args);
                                goto FREE_ADDR;
                        }
                }
                token = strtok(NULL, " ");
        }

        if (rtnl_addr_delete(sock, addr, 0) < 0) {
                CNL_LOG_ERROR("%s: Unable to del addr \n", args);
                goto FREE_ADDR;
        }
        err = CNL_STATUS_SUCCESS;

FREE_ADDR:
        rtnl_addr_put(addr);
        nl_cache_free(link_cache);
        nl_cache_free(addr_cache);

FREE_SOCKET:
        nl_socket_free(sock);

        free(str);
        return err;
}

/**
 * route_add
 * @args: routing configuration [default, device, table, via]
 *
 * Add routing table entry
 */
libnet_status route_add(char *args)
{
        struct nl_sock *sock;
        struct rtnl_route *route;
        struct nl_cache *link_cache;
        libnet_status err = CNL_STATUS_FAILURE;

        char *str = strdup(args);
        char *token;
        char nexthop[64]={0};

        sock = libnet_alloc_socket();
        if (sock == NULL) {
                CNL_LOG_ERROR("Unable to allocate memory for socket\n");
                return err;
        }

        if (libnet_connect(sock, NETLINK_ROUTE) < 0) {
                CNL_LOG_ERROR("Unable to connect socket\n");
                goto FREE_SOCKET;
        }

        link_cache = libnet_link_alloc_cache(sock);
        route = libnet_route_alloc();

        token = strtok(str, " ");
        while( token != NULL) {
                if(0 == strcmp(token, "default")) {
                        if (libnet_route_parse_dst(route, token) != 0) {
                                CNL_LOG_ERROR("%s: Unable to parse dst field\n", args);
                                goto FREE_ROUTE;
                        }
                } else if(0 == strcmp(token, "dev")) {
                        token = strtok(NULL, " ");
                        if (token != NULL) {
                                strcat(nexthop,"dev=");
                                strcat(nexthop,token);
                                strcat(nexthop,",");
                        }
                } else if(0 == strcmp(token, "via")) {
                        token = strtok(NULL, " ");
                        if (token != NULL) {
                                strcat(nexthop,"via=");
                                strcat(nexthop,token);
                                strcat(nexthop,",");
                        }
                } else if(0 == strcmp(token, "metric")) {
                        token = strtok(NULL, " ");
                        if (token != NULL) {
                                strcat(nexthop,"metric=");
                                strcat(nexthop,token);
                                strcat(nexthop,",");
                        }
                } else if(0 == strcmp(token, "table")) {
                        token = strtok(NULL, " ");
                        if (token != NULL) {
                                if (libnet_route_parse_table(route, token) != 0) {
                                        CNL_LOG_ERROR("%s: Unable to parse table field\n", args);
                                        goto FREE_ROUTE;
                                }
                        }
                } else if(0 == strcmp(token, "-4") || 0 == strcmp(token, "4") ||
                          0 == strcmp(token, "inet")) {
                        if (rtnl_route_set_family(route, AF_INET) < 0) {
                                CNL_LOG_ERROR("%s: Unable to set V4 addr\n", args);
                                goto FREE_ROUTE;
                        }
                } else if(0 == strcmp(token, "-6") || 0 == strcmp(token, "6") ||
                          0 == strcmp(token, "inet6")) {
                        if (rtnl_route_set_family(route, AF_INET6) < 0) {
                                CNL_LOG_ERROR("%s: Unable to set V6 addr\n", args);
                                goto FREE_ROUTE;
                        }
                } else {
                        if (libnet_route_parse_dst(route, token) != 0) {
                                CNL_LOG_ERROR("%s: Unable to parse dst field\n", args);
                                goto FREE_ROUTE;
                        }
                }
                token = strtok(NULL, " ");
        }

        if (libnet_route_parse_nexthop(route, nexthop, link_cache) != 0) {
                CNL_LOG_ERROR("%s: Unable to parse nexthop field\n", args);
                goto FREE_ROUTE;
        }

        if (rtnl_route_add(sock, route, NLM_F_EXCL) < 0) {
                CNL_LOG_ERROR("%s: Unable to add route\n", args);
                goto FREE_ROUTE;
        }
        err = CNL_STATUS_SUCCESS;

FREE_ROUTE:
        rtnl_route_put(route);
        nl_cache_free(link_cache);

FREE_SOCKET:
        nl_socket_free(sock);

        free(str);
        return err;
}

static void route_delete_cb(struct nl_object *obj, void *arg)
{
        struct rtnl_route *route = (struct rtnl_route *) obj;
        struct nl_sock *sock = (struct nl_sock *) arg;
        libnet_status err = CNL_STATUS_SUCCESS;

        if ((err = rtnl_route_delete(sock, route, 0)) < 0)
                CNL_LOG_ERROR("Unable to delete route - Returned err = %d\n", err);
}

/**
 * route_delete
 * @args: routing configuration [default, device, table, via]
 *
 * Delete routing table entry
 */
libnet_status route_delete(char *args)
{
        static struct nl_sock *sock;
        struct nl_cache *route_cache;
        struct rtnl_route *route;
        libnet_status err = CNL_STATUS_FAILURE;
        char *str = strdup(args);
        char *token;
        char nexthop[64]={0};

        sock = libnet_alloc_socket();
        if (sock == NULL) {
                CNL_LOG_ERROR("Unable to allocate memory for socket\n");
                return err;
        }

        if (libnet_connect(sock, NETLINK_ROUTE) < 0) {
                CNL_LOG_ERROR("Unable to connect socket\n");
                goto FREE_SOCKET;
        }

        route_cache = libnet_route_alloc_cache(sock, 0);
        route = libnet_route_alloc();

        token = strtok(str, " ");
        while( token != NULL) {
                if(0 == strcmp(token, "default")) {
                        if (libnet_route_parse_dst(route, token) != 0) {
                                CNL_LOG_ERROR("%s: Unable to parse dst field\n", args);
                                goto FREE_ROUTE;
                        }
                } else if(0 == strcmp(token, "dev")) {
                        token = strtok(NULL, " ");
                        if (token != NULL) {
                                strcat(nexthop,"dev=");
                                strcat(nexthop,token);
                                strcat(nexthop,",");
                        }
                } else if(0 == strcmp(token, "via")) {
                        token = strtok(NULL, " ");
                        if (token != NULL) {
                                strcat(nexthop,"via=");
                                strcat(nexthop,token);
                                strcat(nexthop,",");
                        }
                } else if(0 == strcmp(token, "metric")) {
                        token = strtok(NULL, " ");
                        if (token != NULL) {
                                strcat(nexthop,"metric=");
                                strcat(nexthop,token);
                                strcat(nexthop,",");
                        }
                } else if(0 == strcmp(token, "table")) {
                        token = strtok(NULL, " ");
                        if (token != NULL) {
                                if (libnet_route_parse_table(route, token) != 0) {
                                        CNL_LOG_ERROR("%s: Unable to parse table field\n", args);
                                        goto FREE_ROUTE;
                                }
                        }
                } else if(0 == strcmp(token, "-4") || 0 == strcmp(token, "4") ||
                          0 == strcmp(token, "inet")) {
                        if (rtnl_route_set_family(route, AF_INET) < 0) {
                                CNL_LOG_ERROR("%s: Unable to set V4 addr\n", args);
                                goto FREE_ROUTE;
                        }
                } else if(0 == strcmp(token, "-6") || 0 == strcmp(token, "6") ||
                          0 == strcmp(token, "inet6")) {
                        if (rtnl_route_set_family(route, AF_INET6) < 0) {
                                CNL_LOG_ERROR("%s: Unable to set V6 addr\n", args);
                                goto FREE_ROUTE;
                        }
                } else {
                        if (libnet_route_parse_dst(route, token) != 0) {
                                CNL_LOG_ERROR("%s: Unable to parse dst field\n", args);
                                goto FREE_ROUTE;
                        }
                }
                token = strtok(NULL, " ");
        }
        nl_cache_foreach_filter(route_cache, OBJ_CAST(route), route_delete_cb, (void *)sock);
        err = CNL_STATUS_SUCCESS;
FREE_ROUTE:
        nl_cache_free(route_cache);
        rtnl_route_put(route);
FREE_SOCKET:
        nl_socket_free(sock);
        free(str);

        return err;
}

/**
 * rule_add
 * @args: policy routing configuration [input interface, output interface, table]
 *
 * Add policy routing table entry
 */
libnet_status rule_add(char *args)
{
        char *str = strdup(args);
        char *token;

        struct nl_sock *sock;
        struct rtnl_rule *rule;
        libnet_status err = CNL_STATUS_FAILURE;
        int family = AF_UNSPEC;
        struct nl_addr *src, *dst;

        sock = libnet_alloc_socket();
        if (sock == NULL) {
                CNL_LOG_ERROR("Unable to allocate memory for socket\n");
                return err;
        }

        if (libnet_connect(sock, NETLINK_ROUTE) < 0) {
                CNL_LOG_ERROR("Unable to connect socket\n");
                goto FREE_SOCKET;
        }

        rule = libnet_rule_alloc();

        if (rule == NULL) {
                CNL_LOG_ERROR("Unable to allocate memory for rule\n");
                goto FREE_SOCKET;
        }

        if (libnet_addr_parse("all", family, &src) != 0) {
                goto FREE_RULE;
        }

        if (rtnl_rule_set_src(rule, src) < 0) {
                CNL_LOG_ERROR("Unable to set rule src\n");
                goto FREE_RULE;
        }

        rtnl_rule_set_action(rule, RTN_UNICAST);

        token = strtok(str, " ");
        while( token != NULL) {
                if(0 == strcmp(token, "from")) {
                        token = strtok(NULL, " ");
                        if (token != NULL) {
                                if (libnet_addr_parse(token, family, &src) != 0) {
                                        CNL_LOG_ERROR("Unable to parse rule src\n");
                                        goto FREE_RULE;
                                }
                                if (rtnl_rule_set_src(rule, src) < 0) {
                                        CNL_LOG_ERROR("Unable to set rule src\n");
                                        goto FREE_RULE;
                                }
                        } else {
                                CNL_LOG_ERROR("'from' option not provided\n");
                                goto FREE_RULE;
                        }
                } else if (0 == strcmp(token, "to")) {
                        token = strtok(NULL, " ");
                        if (token != NULL) {
                                if (libnet_addr_parse(token, family, &dst) != 0) {
                                        CNL_LOG_ERROR("Unable to parse rule dst\n");
                                        goto FREE_RULE;
                                }
                                if (rtnl_rule_set_dst(rule, dst) < 0) {
                                        CNL_LOG_ERROR("Unable to set rule dst\n");
                                        goto FREE_RULE;
                                }
                        } else {
                                CNL_LOG_ERROR("'to' option not provided\n");
                                goto FREE_RULE;
                        }
                } else if(0 == strcmp(token, "iif")) {
                        token = strtok(NULL, " ");
                        if (token != NULL) {
                                if (rtnl_rule_set_iif(rule, token) < 0) {
                                        CNL_LOG_ERROR("Unable to set rule iif\n");
                                        goto FREE_RULE;
                                }
                        } else {
                                CNL_LOG_ERROR("'iif' option not provided\n");
                                goto FREE_RULE;
                        }
                } else if(0 == strcmp(token, "oif")) {
                        token = strtok(NULL, " ");
                        if (token != NULL) {
                                if (rtnl_rule_set_oif(rule, token) < 0) {
                                        CNL_LOG_ERROR("Unable to set rule oif\n");
                                        goto FREE_RULE;
                                }
                        } else {
                                CNL_LOG_ERROR("'oif' option not provided\n");
                                goto FREE_RULE;
                        }
                } else if(0 == strcmp(token, "lookup") ||
                          0 == strcmp(token, "table")) {
                        token = strtok(NULL, " ");
                        if (token != NULL) {
                                if (rtnl_route_read_table_names(
                                            "/etc/iproute2/rt_tables") < 0) {
                                        CNL_LOG_ERROR("Failed to read %s\n",
                                                      "/etc/iproute2/rt_tables");
                                        goto FREE_RULE;
                                }
                                int tableId = rtnl_route_str2table(token);
                                if (tableId < 0) {
                                        CNL_LOG_ERROR("No such table %s\n", token);
                                        goto FREE_RULE;
                                }
                                rtnl_rule_set_table(rule, tableId);
                                rtnl_rule_set_action(rule, FR_ACT_TO_TBL);
                        } else {
                                CNL_LOG_ERROR("'table/lookup' option not provided\n");
                                goto FREE_RULE;
                        }
                } else if (0 == strcmp(token, "prio")) {
                        token = strtok(NULL, " ");
                        if (token != NULL) {
                                rtnl_rule_set_prio(rule, atoi(token));
                        } else {
                                CNL_LOG_ERROR("'priority' option not provided\n");
                        }
                } else if(0 == strcmp(token, "-4") || 0 == strcmp(token, "4") ||
                          0 == strcmp(token, "inet")) {
                        family = AF_INET;
                        rtnl_rule_set_family(rule, family);
                } else if(0 == strcmp(token, "-6") || 0 == strcmp(token, "6") ||
                          0 == strcmp(token, "inet6")) {
                        family = AF_INET6;
                        rtnl_rule_set_family(rule, family);
                }
                token = strtok(NULL, " ");
        }

        if (rtnl_rule_add(sock, rule, NLM_F_EXCL) < 0) {
                CNL_LOG_ERROR("Unable to add rule\n");
                goto FREE_RULE;
        }
        err = CNL_STATUS_SUCCESS;

FREE_RULE:
        rtnl_rule_put(rule);

FREE_SOCKET:
        nl_socket_free(sock);

        free(str);
        return err;
}

static void rule_delete_cb(struct nl_object *obj, void *arg)
{
        struct rtnl_rule *rule = (struct rtnl_rule *) obj;
        struct nl_sock *sock = (struct nl_sock *) arg;
        libnet_status err = CNL_STATUS_SUCCESS;

        if ((err = rtnl_rule_delete(sock, rule, 0)) < 0)
                CNL_LOG_ERROR("Unable to delete rule - Returned err %d\n", err);
}

/**
 * rule_delete
 * @args: policy routing configuration [input interface, output interface, table]
 *
 * Remove policy routing table entry
 */
libnet_status rule_delete(char *args)
{
        char *str = strdup(args);
        char *token;
        int family = AF_UNSPEC;

        static struct nl_sock *sock;
        struct nl_cache *rule_cache;
        struct rtnl_rule *rule;
        libnet_status err = CNL_STATUS_FAILURE;

        sock = libnet_alloc_socket();
        if (sock == NULL) {
                CNL_LOG_ERROR("Unable to allocate memory for socket\n");
                free(str);
                return err;
        }

        if (libnet_connect(sock, NETLINK_ROUTE) < 0) {
                CNL_LOG_ERROR("Unable to connect socket\n");
                goto FREE_SOCKET;
        }

        rule_cache = libnet_rule_alloc_cache(sock);
        rule = libnet_rule_alloc();

        if (rule == NULL) {
                CNL_LOG_ERROR("Unable to allocate memory for rule\n");
                goto FREE_SOCKET;
        }

        token = strtok(str, " ");
        while( token != NULL) {
                if(0 == strcmp(token, "from")) {
                        token = strtok(NULL, " ");
                        if (token != NULL) {
                                struct nl_addr *a;
                                if (libnet_addr_parse(token, family, &a) != 0) {
                                        CNL_LOG_ERROR("Unable to parse rule src addr\n");
                                        goto FREE_RULE;
                                }
                                rtnl_rule_set_src(rule, a);
                        }
                } else if (0 == strcmp(token, "to")) {
                        token = strtok(NULL, " ");
                        if (token != NULL) {
                                struct nl_addr *b;
                                if (libnet_addr_parse(token, family, &b) != 0) {
                                        CNL_LOG_ERROR("Unable to parse rule dst addr\n");
                                        goto FREE_RULE;
                                }
                                if (rtnl_rule_set_dst(rule, b) < 0) {
                                        CNL_LOG_ERROR("Unable to set rule dst\n");
                                        goto FREE_RULE;
                                }
                        }
                } else if(0 == strcmp(token, "iif")) {
                        token = strtok(NULL, " ");
                        if (token != NULL) {
                                rtnl_rule_set_iif(rule, token);
                        }
                } else if(0 == strcmp(token, "oif")) {
                        token = strtok(NULL, " ");
                        if (token != NULL) {
                                rtnl_rule_set_oif(rule, token);
                        }
                } else if(0 == strcmp(token, "lookup") ||
                          0 == strcmp(token, "table")) {
                        token = strtok(NULL, " ");
                        if (token != NULL) {
                                int tableId = rtnl_route_str2table(token);
                                if (tableId < 0) {
                                        CNL_LOG_ERROR("No such table %s\n", token);
                                        goto FREE_RULE;
                                }
                                rtnl_rule_set_table(rule, tableId);
                                rtnl_rule_set_action(rule, FR_ACT_TO_TBL);
                        }
                } else if(0 == strcmp(token, "-4") || 0 == strcmp(token, "4") ||
                          0 == strcmp(token, "inet")) {
                        family = AF_INET;
                        rtnl_rule_set_family(rule, family);
                } else if(0 == strcmp(token, "-6") || 0 == strcmp(token, "6") ||
                          0 == strcmp(token, "inet6")) {
                        family = AF_INET6;
                        rtnl_rule_set_family(rule, family);
                }
                token = strtok(NULL, " ");
        }
        nl_cache_foreach_filter(rule_cache, OBJ_CAST(rule), rule_delete_cb, (void *)sock);
        err = CNL_STATUS_SUCCESS;

FREE_RULE:
        rtnl_rule_put(rule);

FREE_CACHE:
        nl_cache_free(rule_cache);

FREE_SOCKET:
        nl_socket_free(sock);

        free(str);
        return err;
}

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
                                const char *encaplimit)
{
        struct nl_cache *link_cache;
        struct rtnl_link *link;
        struct in6_addr addr;
        struct nl_sock *sk;
        int if_index;
        libnet_status err = CNL_STATUS_FAILURE;

        sk = libnet_alloc_socket();
        if (sk == NULL) {
                CNL_LOG_ERROR("Unable to allocate memory for socket\n");
                return err;
        }

        if (libnet_connect(sk, NETLINK_ROUTE) < 0) {
                CNL_LOG_ERROR("Unable to connect socket\n");
                goto FREE_SOCKET;
        }

        if (rtnl_link_alloc_cache(sk, AF_UNSPEC, &link_cache) < 0) {
                CNL_LOG_ERROR("Unable to allocate cache\n");
                goto FREE_SOCKET;
        }

        if_index = rtnl_link_name2i(link_cache, dev_name);
        if (!if_index) {
                CNL_LOG_ERROR("Unable to lookup %s\n", dev_name);
                goto FREE_CACHE;
        }

        link = rtnl_link_ip6_tnl_alloc();
        if(!link) {
                CNL_LOG_ERROR("Unable to allocate link\n");
                goto FREE_CACHE;
        }

        rtnl_link_set_name(link, tunnel_name);
        if (rtnl_link_ip6_tnl_set_link(link, if_index) < 0) {
                CNL_LOG_ERROR("Unable to set tunnel interface index\n");
                goto FREE_LINK;
        }

        if (inet_pton(AF_INET6, local_ip6, &addr) != 1) {
                // inet_pton returns failure if retVal is not 1
                CNL_LOG_ERROR("Invalid local IPV6 address\n");
                goto FREE_LINK;
        }

        if (rtnl_link_ip6_tnl_set_local(link, &addr) < 0) {
                CNL_LOG_ERROR("Unable to set tunnel local address\n");
                goto FREE_LINK;
        }

        if (inet_pton(AF_INET6, remote_ip6, &addr) != 1) {
                // inet_pton returns failure if retVal is not 1
                CNL_LOG_ERROR("Invalid remote IPV6 address\n");
                goto FREE_LINK;
        }

        if (rtnl_link_ip6_tnl_set_remote(link, &addr) < 0) {
                CNL_LOG_ERROR("Unable to set tunnel remote address\n");
                goto FREE_LINK;
        }

        if (rtnl_link_add(sk, link, NLM_F_CREATE | NLM_F_EXCL) < 0) {
                CNL_LOG_ERROR("Unable to add link\n");
                goto FREE_LINK;
        }
        err = CNL_STATUS_SUCCESS;

FREE_LINK:
        rtnl_link_put(link);
FREE_CACHE:
        nl_cache_free(link_cache);
FREE_SOCKET:
        nl_socket_free(sk);

        return err;
}

/**
 * interface_delete
 * @name: interface name
 *
 * Delete an interface
 */
libnet_status interface_delete(char *name)
{
        struct rtnl_link *link;
        struct nl_sock *sk;
        libnet_status err = CNL_STATUS_FAILURE;

        sk = libnet_alloc_socket();
        if (sk == NULL) {
                CNL_LOG_ERROR("Unable to allocate memory for socket\n");
                return err;
        }

        if (libnet_connect(sk, NETLINK_ROUTE) < 0) {
                CNL_LOG_ERROR("Unable to connect socket\n");
                goto FREE_SOCKET;
        }

        link = rtnl_link_alloc();
        rtnl_link_set_name(link, name);

        if (rtnl_link_delete(sk, link) < 0) {
                CNL_LOG_ERROR("Unable to delete link\n");
                goto FREE_LINK;
        }
        err = CNL_STATUS_SUCCESS;

FREE_LINK:
        rtnl_link_put(link);
FREE_SOCKET:
        nl_socket_free(sk);

        return err;
}

/**
 * interface_set_flags
 * @if_name: interface name
 * @flags: netdevice flags
 *
 * Set interface flags
 */
libnet_status interface_set_flags(char *if_name, unsigned int flags)
{
        struct nl_sock *sk;
        struct rtnl_link *link;
        struct rtnl_link *change;
        struct nl_cache *cache;
        libnet_status err = CNL_STATUS_FAILURE;

        sk = libnet_alloc_socket();
        if (sk == NULL) {
                CNL_LOG_ERROR("Unable to allocate memory for socket\n");
                return err;
        }

        if (libnet_connect(sk, NETLINK_ROUTE) < 0) {
                CNL_LOG_ERROR("Unable to connect socket\n");
                goto FREE_SOCKET;
        }

        if (rtnl_link_alloc_cache(sk, AF_UNSPEC, &cache) < 0) {
                CNL_LOG_ERROR("Unable to allocate cache\n");
                goto FREE_SOCKET;
        }

        if (!(link = rtnl_link_get_by_name(cache, if_name))) {
                CNL_LOG_ERROR("Link not found\n");
                goto FREE_CACHE;
        }

        change = rtnl_link_alloc();
        rtnl_link_set_flags(change, flags);

        if (rtnl_link_change(sk, link, change, 0) < 0) {
                CNL_LOG_ERROR("Unable to set flag\n");
                goto FREE_LINK2;
        }
        err = CNL_STATUS_SUCCESS;

FREE_LINK2:
        rtnl_link_put(change);
FREE_LINK:
        rtnl_link_put(link);
FREE_CACHE:
        nl_cache_free(cache);
FREE_SOCKET:
        nl_socket_free(sk);

        return err;
}

/**
 * interface_rename
 * @if_name: interface name
 * @new_name: netdevice flags
 *
 * Rename interface
 * This operation is not recommended if the device is running or
 * has some addresses already configured.
 */
libnet_status interface_rename(char *if_name, char *new_name)
{
        struct nl_sock *sk;
        struct rtnl_link *link;
        struct rtnl_link *change;
        struct nl_cache *cache;
        libnet_status err = CNL_STATUS_FAILURE;

        sk = libnet_alloc_socket();
        if (sk == NULL) {
                CNL_LOG_ERROR("Unable to allocate memory for socket\n");
                return err;
        }

        if (libnet_connect(sk, NETLINK_ROUTE) < 0) {
                CNL_LOG_ERROR("Unable to connect socket\n");
                goto FREE_SOCKET;
        }

        if (rtnl_link_alloc_cache(sk, AF_UNSPEC, &cache) < 0) {
                CNL_LOG_ERROR("Unable to allocate cache\n");
                goto FREE_SOCKET;
        }

        if (!(link = rtnl_link_get_by_name(cache, if_name))) {
                CNL_LOG_ERROR("Interface not found\n");
                goto FREE_CACHE;
        }

        change = rtnl_link_alloc();
        rtnl_link_set_name(link, new_name);

        if (rtnl_link_change(sk, link, change, 0) < 0) {
                CNL_LOG_ERROR("Unable to change name\n");
                goto FREE_LINK2;
        }
        err = CNL_STATUS_SUCCESS;

FREE_LINK2:
        rtnl_link_put(change);
        rtnl_link_put(link);
FREE_CACHE:
        nl_cache_free(cache);
FREE_SOCKET:
        nl_socket_free(sk);
        return err;
}

/**
 * neighbour_delete
 * @dev: interface name
 * @ip: ip address
 *
 * Delete an entry in neighbour table
 */
libnet_status neighbour_delete(char *dev, char *ip)
{
        struct nl_sock *sock;
        struct rtnl_neigh *neigh;
        struct nl_cache *link_cache;
        libnet_status err = CNL_STATUS_FAILURE;

        sock = libnet_alloc_socket();

        if (sock == NULL) {
                CNL_LOG_ERROR("Unable to allocate memory for socket\n");
                return err;
        }

        if (libnet_connect(sock, NETLINK_ROUTE) < 0) {
                CNL_LOG_ERROR("Unable to connect socket\n");
                goto FREE_SOCKET;
        }

        link_cache = libnet_link_alloc_cache(sock);
        neigh = libnet_neigh_alloc();

        if (libnet_neigh_parse_dev(neigh, link_cache, dev) != 0) {
                CNL_LOG_ERROR("Unable to parse dev field\n");
                goto FREE_CACHE;
        }

        if (libnet_neigh_parse_dst(neigh, ip) != 0) {
                CNL_LOG_ERROR("Unable to parse dst field\n");
                goto FREE_CACHE;
        }

        if (rtnl_neigh_delete(sock, neigh, 0) < 0) {
                CNL_LOG_ERROR("Unable to delete neighbour\n");
                goto FREE_CACHE;
        }
        err = CNL_STATUS_SUCCESS;

FREE_CACHE:
        rtnl_neigh_put(neigh);
        nl_cache_put(link_cache);

FREE_SOCKET:
        nl_socket_free(sock);

        return err;
}

static void bridge_get_slave_name_cb(struct nl_object *match, void *arg)
{
        uint32_t ifindex;
        struct bridge_info *bridge = arg;
        char name[IFNAMSIZ] = {0};

        ifindex = rtnl_link_get_ifindex((struct rtnl_link *)match);
        if (0 >= ifindex)
                return;
        if (!rtnl_link_i2name(bridge->link_cache, ifindex, name, sizeof(name)))
                CNL_LOG_ERROR("Interface index %d does not exist\n",
                              ifindex);
        bridge->slave_name[bridge->slave_count++] = strdup(name);
}

/**
 * bridge_get_info
 * @bridge_name: bridge interface name
 * @bridge: bridge structure to fill
 * Get bridge details
 */
libnet_status bridge_get_info(char *bridge_name, struct bridge_info *bridge)
{
        struct nl_sock *sk;
        struct nl_cache *link_cache;
        struct rtnl_link *link;
        uint32_t ifindex;
        libnet_status err = CNL_STATUS_FAILURE;
        errno_t rc = -1;

        rc = memset_s(bridge, sizeof(struct bridge_info), 0, sizeof(struct bridge_info));
        ERR_CHK(rc);
        if (rc < EOK)
                return CNL_STATUS_FAILURE;

        sk = libnet_alloc_socket();
        if (sk == NULL) {
                CNL_LOG_ERROR("Unable to allocate memory for socket\n");
                return err;
        }

        if (libnet_connect(sk, NETLINK_ROUTE) < 0) {
                CNL_LOG_ERROR("Unable to connect socket\n");
                goto FREE_SOCKET;
        }

        if (rtnl_link_alloc_cache(sk, AF_UNSPEC, &link_cache) < 0) {
                CNL_LOG_ERROR("Unable to allocate cache\n");
                goto FREE_SOCKET;
        }

        if (!(ifindex = rtnl_link_name2i(link_cache, bridge_name))) {
                CNL_LOG_ERROR("Interface %s does not exist\n", bridge_name);
                goto FREE_CACHE;
        }

        link = rtnl_link_alloc();
        rtnl_link_set_master(link, ifindex);

        bridge->link_cache = link_cache;
        nl_cache_foreach_filter(link_cache, OBJ_CAST(link), bridge_get_slave_name_cb,
                                (void *)bridge);

        rtnl_link_put(link);
        err = CNL_STATUS_SUCCESS;
FREE_CACHE:
        nl_cache_free(link_cache);
        bridge->link_cache = NULL;

FREE_SOCKET:
        nl_socket_free(sk);

        return err;
}

/**
 * bridge_free_info
 * @bridge: bridge_info memory to free
 * Free bridge_info members
 */
void bridge_free_info(struct bridge_info *bridge)
{
        while (0 != bridge->slave_count) {
                free(bridge->slave_name[bridge->slave_count-1]);
                bridge->slave_name[bridge->slave_count-1] = NULL;
                bridge->slave_count--;
        }
}

static void neighbour_get_cb(struct nl_object *match, void *arg)
{
        struct neighbour_info *neigh_info = arg;
        char buf[40] = {0};

        if (MAX_NEIGH_COUNT < neigh_info->neigh_count)
                return;

        nl_addr2str(rtnl_addr_get_local((struct rtnl_addr *) match), buf, sizeof(buf));
        neigh_info->neigh_arr[neigh_info->neigh_count].local = strdup(buf);

        nl_addr2str(rtnl_neigh_get_lladdr((struct rtnl_neigh *) match), buf, sizeof(buf));
        neigh_info->neigh_arr[neigh_info->neigh_count].mac = strdup(buf);

        int state = rtnl_neigh_get_state((struct rtnl_neigh *) match);

        // Filter out entries with NONE / NOARP / NUD_PERMANENT
        // NUD_NOARP and NUD_PERMANENT are pseudostates
        if (state == NUD_NONE || state == NUD_NOARP || state == NUD_PERMANENT) {
                if (neigh_info->neigh_arr[neigh_info->neigh_count].local != NULL) {
                        free(neigh_info->neigh_arr[neigh_info->neigh_count].local);
                        neigh_info->neigh_arr[neigh_info->neigh_count].local = NULL;
                }

                if (neigh_info->neigh_arr[neigh_info->neigh_count].mac != NULL) {
                        free(neigh_info->neigh_arr[neigh_info->neigh_count].mac);
                        neigh_info->neigh_arr[neigh_info->neigh_count].mac = NULL;
                }
                return;
        }

        neigh_info->neigh_arr[neigh_info->neigh_count].state = state;

        neigh_info->neigh_count++;

}

/**
 * neighbour_get_list - lists all entries except for NONE, NOARP and NUD_PERMANENT
 * @arr: array to fill neighbour table
 * Get bridge details
 */
libnet_status neighbour_get_list(struct neighbour_info *arr)
{
        struct nl_sock *sock;
        struct rtnl_neigh *neigh;
        struct nl_cache *link_cache;
        struct nl_cache *neigh_cache;
        libnet_status err = CNL_STATUS_FAILURE;
        errno_t rc = -1;

        rc = memset_s(arr, sizeof(struct neighbour_info), 0, sizeof(struct neighbour_info));
        ERR_CHK(rc);
        if (rc < EOK)
                return CNL_STATUS_FAILURE;

        sock = libnet_alloc_socket();
        if (sock == NULL) {
                CNL_LOG_ERROR("Unable to allocate memory for socket\n");
                return err;
        }
        if (libnet_connect(sock, NETLINK_ROUTE) < 0) {
                CNL_LOG_ERROR("Unable to connect socket\n");
                goto FREE_SOCKET;
        }

        neigh_cache = libnet_neigh_alloc_cache(sock);
        neigh = libnet_neigh_alloc();

        nl_cache_foreach_filter(neigh_cache, OBJ_CAST(neigh),
                                neighbour_get_cb, (void *)arr);

        rtnl_neigh_put(neigh);
        nl_cache_put(neigh_cache);
        err = CNL_STATUS_SUCCESS;
FREE_SOCKET:
        nl_socket_free(sock);

        return err;
}

/**
 * neighbour_free_neigh
 * @neigh_info: neighbour table structure
 * Free neighbour_info members
 */
void neighbour_free_neigh(struct neighbour_info *neigh_info)
{
        while (0 != neigh_info->neigh_count) {
                free(neigh_info->neigh_arr[neigh_info->neigh_count-1].local);
                neigh_info->neigh_arr[neigh_info->neigh_count-1].local = NULL;

                free(neigh_info->neigh_arr[neigh_info->neigh_count-1].mac);
                neigh_info->neigh_arr[neigh_info->neigh_count-1].mac = NULL;

                neigh_info->neigh_count--;
        }
}

/**
 * interface_get_stats
 * @ifStatsMask: Get the interface needed information by bitmask
 * @if_name: Name of the interface for information
 * @stats: Structure for interface stats
 *
 * Added for getting interface statistics
 */
libnet_status interface_get_stats(cnl_ifstats_mask ifstats_mask, const char* if_name, cnl_iface_stats *stats)
{
        struct nl_sock *sock;
        struct nl_cache *link_cache;
        struct rtnl_link *link;
        libnet_status err = CNL_STATUS_FAILURE;

        sock = nl_socket_alloc();
        if (sock == NULL) {
                CNL_LOG_ERROR("Unable to allocate memory for socket\n");
                return err;
        }

        if (nl_connect(sock, NETLINK_ROUTE) < 0) {
                CNL_LOG_ERROR("Unable to connect socket\n");
                goto FREE_SOCKET;
        }

        if (rtnl_link_alloc_cache(sock, AF_UNSPEC, &link_cache) < 0) {
                CNL_LOG_ERROR("Unable to allocate cache\n");
                goto FREE_SOCKET;
        }

        link = rtnl_link_get_by_name(link_cache, if_name);
        if (!link) {
                CNL_LOG_ERROR("Unable to find the interface %s\n", if_name);
                goto FREE_CACHE;
        }

        if ((ifstats_mask & IFSTAT_RXTX_PACKET))
        {
                stats->rx_packet = rtnl_link_get_stat(link, RTNL_LINK_RX_PACKETS);
                stats->tx_packet = rtnl_link_get_stat(link, RTNL_LINK_TX_PACKETS);
                err = CNL_STATUS_SUCCESS;
        }

        if ((ifstats_mask & IFSTAT_RXTX_BYTES))
        {
                stats->rx_bytes = rtnl_link_get_stat(link, RTNL_LINK_RX_BYTES);
                stats->tx_bytes = rtnl_link_get_stat(link, RTNL_LINK_TX_BYTES);
                err = CNL_STATUS_SUCCESS;
        }

        if ((ifstats_mask & IFSTAT_RXTX_ERRORS))
        {
                stats->rx_errors = rtnl_link_get_stat(link, RTNL_LINK_RX_ERRORS);
                stats->tx_errors = rtnl_link_get_stat(link, RTNL_LINK_TX_ERRORS);
                err = CNL_STATUS_SUCCESS;
        }

        if ((ifstats_mask & IFSTAT_RXTX_DROPPED))
        {
                stats->rx_dropped = rtnl_link_get_stat(link, RTNL_LINK_RX_DROPPED);
                stats->tx_dropped = rtnl_link_get_stat(link, RTNL_LINK_TX_DROPPED);
                err = CNL_STATUS_SUCCESS;
        }

FREE_LINK:
        rtnl_link_put(link);
FREE_CACHE:
        nl_cache_free(link_cache);
FREE_SOCKET:
        nl_socket_free(sock);

        return err;
}
