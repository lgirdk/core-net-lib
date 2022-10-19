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

#include <errno.h>

#include <netlink/route/neighbour.h>
#include <netlink/route/link.h>
#include <netlink/route/addr.h>
#include <netlink/route/route.h>
#include <netlink/route/rule.h>

#include "safec_lib_common.h"

#include "libnet_util.h"

/************** Common helpers ******************/

int libnet_connect(struct nl_sock *socket, int proto)
{
        int err = 0;

        err = nl_connect(socket, proto);
        if (err < 0) {
                fprintf(stderr, "Failed to connect netlink socket");
        }

        return err;
}

struct nl_sock *libnet_alloc_socket(void)
{
        struct nl_sock *socket;

        socket = nl_socket_alloc();
        if (socket == NULL) {
                fprintf(stderr, "Failed to allocate netlink socket");
                return NULL;
        }

        return socket;
}

int libnet_addr_parse(const char *str, int family, struct nl_addr **addr)
{
        int err = 0;

        err = nl_addr_parse(str, family, addr);
        if (err < 0) {
                fprintf(stderr, "Failed to parse address");
                return err;
        }

        return err;
}

struct nl_cache *libnet_alloc_cache(struct nl_sock *socket, const char *name,
                                    int (*ac)(struct nl_sock *, struct nl_cache **))
{
        struct nl_cache *cache;
        int err;

        err = ac(socket, &cache);
        if (err < 0) {
                fprintf(stderr, "Failed to allocate %s cache: %s",
                        name, nl_geterror(err));
                return NULL;
        }

        nl_cache_mngt_provide(cache);

        return cache;
}

struct nl_cache *libnet_alloc_cache_flags(struct nl_sock *socket,
                                          const char *name, unsigned int flags,
                                          int (*ac)(struct nl_sock *, struct nl_cache **,
                                                    unsigned int))
{
        struct nl_cache *cache;
        int err;

        err = ac(socket, &cache, flags);
        if (err < 0) {
                fprintf(stderr, "Failed to allocate %s cache: %s",
                        name, nl_geterror(err));
                return NULL;
        }

        nl_cache_mngt_provide(cache);

        return cache;
}

/************** Link helpers ******************/

struct rtnl_link *libnet_link_alloc(void)
{
        struct rtnl_link *rt_link;

        rt_link = rtnl_link_alloc();
        if (rt_link == NULL) {
                fprintf(stderr, "Failed to allocate link object");
                return NULL;
        }

        return rt_link;
}

struct nl_cache *libnet_link_alloc_cache_family_flags(struct nl_sock *socket,
                                                      int family,
                                                      unsigned int flags)
{
        struct nl_cache *cache;
        int err;

        err = rtnl_link_alloc_cache_flags(socket, family, &cache, flags);
        if (err < 0) {
                fprintf(stderr, "Failed to allocate link cache");
                return NULL;
        }

        nl_cache_mngt_provide(cache);

        return cache;
}

struct nl_cache *libnet_link_alloc_cache_family(struct nl_sock *socket, int family)
{
        return libnet_link_alloc_cache_family_flags(socket, family, 0);
}

struct nl_cache *libnet_link_alloc_cache(struct nl_sock *socket)
{
        return libnet_link_alloc_cache_family(socket, AF_UNSPEC);
}

struct nl_cache *libnet_link_alloc_cache_flags(struct nl_sock *socket,
                                               unsigned int flags)
{
        return libnet_link_alloc_cache_family_flags(socket, AF_UNSPEC, flags);
}

/************** Address helpers ******************/

struct rtnl_addr *libnet_addr_alloc(void)
{
        struct rtnl_addr *address;

        address = rtnl_addr_alloc();
        if (address == NULL) {
                fprintf(stderr, "Failed to allocate address object");
                return NULL;
        }

        return address;
}

int libnet_addr_parse_local(struct rtnl_addr *address, char *args)
{
        struct nl_addr *nl_a;
        int err = 0;

        err = libnet_addr_parse(args, rtnl_addr_get_family(address), &nl_a);
        if (err != 0) {
                return err;
        }

        err = rtnl_addr_set_local(address, nl_a);
        if (err < 0) {
                fprintf(stderr, "Failed to set local address");
        }

FREE_ADDR:
        nl_addr_put(nl_a);
        return err;
}

int libnet_addr_parse_dev(struct rtnl_addr *address, struct nl_cache *link_cache,
                          char *args)
{
        int ival;
        int err = 0;

        ival = rtnl_link_name2i(link_cache, args);
        if (ival == 0) {
                fprintf(stderr, "Link %s does not exist", args);
                return ENOENT;
        }

        rtnl_addr_set_ifindex(address, ival);
        return err;
}

int libnet_addr_parse_label(struct rtnl_addr *address, char *args)
{
        int err = 0;

        err = rtnl_addr_set_label(address, args);
        if (err < 0) {
                fprintf(stderr, "Failed to set address label");
        }

        return err;
}

int libnet_addr_parse_peer(struct rtnl_addr *address, char *args)
{
        struct nl_addr *nl_a;
        int err = 0;

        err = libnet_addr_parse(args, rtnl_addr_get_family(address), &nl_a);
        if (err != 0) {
                return err;
        }

        err = rtnl_addr_set_peer(address, nl_a);
        if (err < 0)
                fprintf(stderr, "Failed to set peer address");

FREE_ADDR:
        nl_addr_put(nl_a);
        return err;
}

int libnet_addr_parse_broadcast(struct rtnl_addr *address, char *args)
{
        struct nl_addr *nl_a;
        int err = 0;

        err = libnet_addr_parse(args, rtnl_addr_get_family(address), &nl_a);
        if (err != 0) {
                return err;
        }

        err = rtnl_addr_set_broadcast(address, nl_a);
        if (err < 0) {
                fprintf(stderr, "Failed to set broadcast address");
                goto FREE_ADDR;
        }

FREE_ADDR:
        nl_addr_put(nl_a);
        return err;
}

static uint32_t parse_lifetime(const char *args, int *err)
{
        uint64_t msecs;

        if (!strcasecmp(args, "forever"))
                return 0xFFFFFFFFU;

        *err = nl_str2msec(args, &msecs);
        if (*err < 0) {
                fprintf(stderr, "Failed to parse time string");
                return 0;
        }

        *err = 0;
        return (msecs / 1000);
}

int libnet_addr_parse_preferred(struct rtnl_addr *address, char *args)
{
        int err = 0;
        uint32_t lifetime = parse_lifetime(args, &err);

        if (err != 0) {
                return err;
        } else {
                rtnl_addr_set_preferred_lifetime(address, lifetime);
                return 0;
        }
}

int libnet_addr_parse_valid(struct rtnl_addr *address, char *args)
{
        int err = 0;
        uint32_t lifetime = parse_lifetime(args, &err);

        if (err != 0) {
                return err;
        } else {
                rtnl_addr_set_valid_lifetime(address, lifetime);
                return 0;
        }
}

/************** Route helpers ******************/

struct rtnl_route *libnet_route_alloc(void)
{
        struct rtnl_route *rt_route;

        rt_route = rtnl_route_alloc();
        if (rt_route == NULL) {
                fprintf(stderr, "Failed to allocate route object");
                return NULL;
        }

        return rt_route;
}

struct nl_cache *libnet_route_alloc_cache(struct nl_sock *socket, int flags)
{
        struct nl_cache *cache;
        int err;

        err = rtnl_route_alloc_cache(socket, AF_UNSPEC, flags, &cache);
        if (err < 0) {
                fprintf(stderr, "Failed to allocate route cache");
                return NULL;
        }

        nl_cache_mngt_provide(cache);

        return cache;
}

int libnet_route_parse_dst(struct rtnl_route *rt_route, char *args)
{
        struct nl_addr *addr;
        int err = 0;

        err = libnet_addr_parse(args, rtnl_route_get_family(rt_route), &addr);
        if (err != 0) {
                return err;
        }

        err = rtnl_route_set_dst(rt_route, addr);
        if (err < 0) {
                fprintf(stderr, "Failed to set destination address");
                goto FREE_ADDR;
        }

FREE_ADDR:
        nl_addr_put(addr);
        return err;
}

int libnet_route_parse_src(struct rtnl_route *rt_route, char *args)
{
        struct nl_addr *addr;
        int err = 0;

        err = libnet_addr_parse(args, rtnl_route_get_family(rt_route), &addr);
        if (err != 0) {
                return err;
        }

        err = rtnl_route_set_src(rt_route, addr);
        if (err < 0) {
                fprintf(stderr, "Failed to set source address");
                goto FREE_ADDR;
        }

FREE_ADDR:
        nl_addr_put(addr);
        return err;
}

int libnet_route_parse_pref_src(struct rtnl_route *rt_route, char *args)
{
        struct nl_addr *addr;
        int err = 0;

        err = libnet_addr_parse(args, rtnl_route_get_family(rt_route), &addr);
        if (err != 0) {
                return err;
        }

        err = rtnl_route_set_pref_src(rt_route, addr);
        if (err < 0) {
                fprintf(stderr, "Failed to set preferred source address");
                goto FREE_ADDR;
        }

FREE_ADDR:
        nl_addr_put(addr);
        return err;
}

int libnet_route_parse_nexthop(struct rtnl_route *rt_route, char *subopts,
                               struct nl_cache *link_cache)
{
        enum {
                NEXTHOP_DEV,
                NEXTHOP_VIA,
                NEXTHOP_WEIGHT,
                NEXTHOP_AS,
        };
        static char *const tokens[] = {
                "dev",
                "via",
                "weight",
                "as",
                NULL,
        };
        unsigned long ulval;
        struct rtnl_nexthop *nexthop;
        char *args;
        char *end_ptr;
        struct nl_addr *address;
        int int_val;
        int err_val = 0;

        if (!(nexthop = rtnl_route_nh_alloc())) {
                fprintf(stderr, "Out of memory");
                return ENOMEM;
        }

        while (*subopts != '\0') {
                int ret = getsubopt(&subopts, tokens, &args);
                if (ret == -1) {
                        fprintf(stderr, "Unknown nexthop token %s", args);
                        return EINVAL;
                }

                if (args == NULL) {
                        fprintf(stderr, "Missing argument to option %s\n",
                                tokens[ret]);
                        return EINVAL;
                }

                switch (ret) {
                case NEXTHOP_DEV:
                        if (!(int_val = rtnl_link_name2i(link_cache, args))) {
                                fprintf(stderr, "Link %s does not exist", args);
                                return ENOENT;
                        }

                        rtnl_route_nh_set_ifindex(nexthop, int_val);
                        break;

                case NEXTHOP_VIA:
                        if (rtnl_route_get_family(rt_route) == AF_MPLS) {
                                if ((err_val = libnet_addr_parse(args, 0, &address)) != 0) {
                                        return err_val;
                                }
                                rtnl_route_nh_set_via(nexthop, address);
                        } else {
                                if ((err_val = libnet_addr_parse(args, rtnl_route_get_family(rt_route), &address)) != 0) {
                                        return err_val;
                                }
                                rtnl_route_nh_set_gateway(nexthop, address);
                        }
                        nl_addr_put(address);
                        break;

                case NEXTHOP_AS:
                        if ((err_val = libnet_addr_parse(args, rtnl_route_get_family(rt_route), &address)) != 0) {
                                return err_val;
                        }
                        rtnl_route_nh_set_newdst(nexthop, address);
                        nl_addr_put(address);
                        break;

                case NEXTHOP_WEIGHT:
                        ulval = strtoul(args, &end_ptr, 0);
                        if (end_ptr == args) {
                                fprintf(stderr,
                                        "Invalid weight %s, not numeric",
                                        args);
                                return EINVAL;
                        }
                        rtnl_route_nh_set_weight(nexthop, ulval);
                        break;
                }
        }

        rtnl_route_add_nexthop(rt_route, nexthop);
        return err_val;
}

int libnet_route_parse_table(struct rtnl_route *rt_route, char *args)
{
        unsigned long lval;
        char *endptr;
        int table;
        int err = 0;

        lval = strtoul(args, &endptr, 0);
        if (endptr == args) {
                table = rtnl_route_str2table(args);
                if (table < 0) {
                        fprintf(stderr, "Unknown table name %s", args);
                        return EINVAL;
                }
        }
        else {
                table = lval;
        }

        rtnl_route_set_table(rt_route, table);
        return 0;
}

/************** Rule helpers ******************/

struct rtnl_rule *libnet_rule_alloc(void)
{
        struct rtnl_rule *rt_rule;

        rt_rule = rtnl_rule_alloc();
        if (rt_rule == NULL) {
                fprintf(stderr, "Failed to allocate rule object");
                return NULL;
        }

        return rt_rule;
}

struct nl_cache *libnet_rule_alloc_cache(struct nl_sock *socket)
{
        struct nl_cache *cache;
        int err = 0;

        err = rtnl_rule_alloc_cache(socket, AF_UNSPEC, &cache);
        if (err < 0) {
                fprintf(stderr, "Failed to allocate routing rule cache");
                return NULL;
        }

        nl_cache_mngt_provide(cache);

        return cache;
}

/************** Neighbour helpers ******************/

struct rtnl_neigh *libnet_neigh_alloc(void)
{
        struct rtnl_neigh *rt_neigh;

        rt_neigh = rtnl_neigh_alloc();
        if (rt_neigh == NULL)
                fprintf(stderr, "Failed to allocate neighbour object");
        return rt_neigh;
}

int libnet_neigh_parse_dst(struct rtnl_neigh *rt_neigh, char *args)
{
        int err = 0;
        struct nl_addr *nl_a;

        err = libnet_addr_parse(args, rtnl_neigh_get_family(rt_neigh), &nl_a);
        if (err != 0)
                return err;
        err = rtnl_neigh_set_dst(rt_neigh, nl_a);
        if (err < 0) {
                fprintf(stderr, "Failed to set local address");
                goto FREE_ADDR;
        }
FREE_ADDR:
        nl_addr_put(nl_a);
        return err;
}

int libnet_neigh_parse_dev(struct rtnl_neigh *rt_neigh,
                           struct nl_cache *link_cache, char *args)
{
        int err = ENOENT;
        int ival;

        ival = rtnl_link_name2i(link_cache, args);
        if (ival == 0) {
                fprintf(stderr, "%s does not exist", args);
                return err;
        }
        rtnl_neigh_set_ifindex(rt_neigh, ival);
        err = 0;
        return err;
}
