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

#ifndef _LIBNET_UTILS_H
#define _LIBNET_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

/************** Common helpers ******************/

struct nl_sock *libnet_alloc_socket(void);

int libnet_connect(struct nl_sock *, int);

struct nl_cache *libnet_alloc_cache(struct nl_sock *, const char *,
			int (*ac)(struct nl_sock *, struct nl_cache **));

struct nl_cache *libnet_alloc_cache_flags(struct nl_sock *,
			const char *, unsigned int flags,
			int (*ac)(struct nl_sock *, struct nl_cache **,
				  unsigned int));

/************** Link helpers ******************/

struct rtnl_link *libnet_link_alloc(void);

struct nl_cache *libnet_link_alloc_cache_family_flags(struct nl_sock *sock,
			int family, unsigned int flags);

struct nl_cache *libnet_link_alloc_cache_family(struct nl_sock *sock,
			int family);

struct nl_cache *libnet_link_alloc_cache(struct nl_sock *sock);


/************** Address helpers ******************/

struct rtnl_addr *libnet_addr_alloc(void);

int libnet_addr_parse(const char *, int, struct nl_addr **);

int libnet_addr_parse_local(struct rtnl_addr *addr, char *arg);

int libnet_addr_parse_dev(struct rtnl_addr *addr, struct nl_cache *link_cache, char *arg);

int libnet_addr_parse_broadcast(struct rtnl_addr *addr, char *arg);

int libnet_addr_parse_preferred(struct rtnl_addr *addr, char *arg);

int libnet_addr_parse_valid(struct rtnl_addr *addr, char *arg);

#define libnet_addr_alloc_cache(sk) \
		libnet_alloc_cache((sk), "address", rtnl_addr_alloc_cache)

/************** Route helpers ******************/

struct rtnl_route *libnet_route_alloc(void);

struct nl_cache *libnet_route_alloc_cache(struct nl_sock *sk, int flags);

int libnet_route_parse_dst(struct rtnl_route *route, char *arg);

int libnet_route_parse_nexthop(struct rtnl_route *route, char *subopts,
                   struct nl_cache *link_cache);

int libnet_route_parse_table(struct rtnl_route *route, char *arg);

/************** Rule helpers ******************/

struct rtnl_rule *libnet_rule_alloc(void);

struct nl_cache *libnet_rule_alloc_cache(struct nl_sock *sk);

/************** Neighbour helpers ******************/

struct rtnl_neigh *libnet_neigh_alloc(void);

int libnet_neigh_parse_dst(struct rtnl_neigh *neigh, char *arg);

int libnet_neigh_parse_dev(struct rtnl_neigh *neigh,
                            struct nl_cache *link_cache, char *arg);

#define libnet_neigh_alloc_cache(sk) \
		libnet_alloc_cache_flags((sk), "neighbour", \
					 NL_CACHE_AF_ITER, \
					 rtnl_neigh_alloc_cache_flags)
#endif
