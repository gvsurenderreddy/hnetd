/*
 * $Id: dncp.h $
 *
 * Author: Markus Stenberg <markus stenberg@iki.fi>
 *
 * Copyright (c) 2013 cisco Systems, Inc.
 *
 * Created:       Wed Nov 20 13:15:53 2013 mstenber
 * Last modified: Mon May 25 11:19:55 2015 mstenber
 * Edit time:     173 min
 *
 */

#pragma once

#include "hnetd.h"
#include "tlv.h"

/* in6_addr */
#include <netinet/in.h>

/* IFNAMSIZ */
#include <net/if.h>

/* DNS_MAX_ESCAPED_LEN */
#include "dns_util.h"

#include <libubox/list.h>

/********************************************* Opaque object-like structures */

/* A single dncp instance. */
typedef struct dncp_struct dncp_s, *dncp;

/* A single node in the dncp network. It is effectively TLV list and
 * other associated metadata that should not be visible to users of
 * this public API. If referring to local node, the TLVs visible here
 * are the ones that have been actually published to other nodes
 * (after a delay). */
typedef struct dncp_node_struct dncp_node_s, *dncp_node;

/* generic subscriber event enum */
enum dncp_subscriber_event {
  DNCP_EVENT_REMOVE,
  DNCP_EVENT_ADD,
  DNCP_EVENT_UPDATE
};

/* A single, local published TLV.*/
typedef struct dncp_tlv_struct dncp_tlv_s, *dncp_tlv;

/*
 * Flow of DNCP state change notifications (outbound case):
 *
 * - (if local TLV change), local_tlv_change_callback is called
 * .. at some point, when TLV changes are to be published to the network ..
 * - republish_callback is called
 * - tlv_change_callback is called
 */

enum {
  DNCP_CALLBACK_LOCAL_TLV,
  DNCP_CALLBACK_REPUBLISH,
  DNCP_CALLBACK_TLV,
  DNCP_CALLBACK_NODE,
  DNCP_CALLBACK_LINK,
  DNCP_CALLBACK_SOCKET_MSG,
  NUM_DNCP_CALLBACKS
};

typedef struct dncp_subscriber_struct dncp_subscriber_s, *dncp_subscriber;

struct dncp_subscriber_struct {
  /**
   * Place within list of subscribers (owned by dncp while subscription
   * is valid). Using the same subscriber object twice will result in
   * undefined (and most likely bad) behavior.
   */
  struct list_head lhs[NUM_DNCP_CALLBACKS];

  /**
   * Local TLV change notification.
   *
   * This is called whenever one local set of TLVs (to be published at
   * some point) changes.
   *
   * @param tlv The TLV that is being added or removed (there is no 'update').
   * @param add Flag which indicates whether the operation was add or remove.
   */
  void (*local_tlv_change_callback)(dncp_subscriber s,
                                    struct tlv_attr *tlv, bool add);

  /**
   * About to republish local TLVs notification.
   *
   * This is when TLVs with relative timestamps should be refreshed.
   * It is called _before_ TLV change notifications for _local_ TLVs
   * are provided.
   */
  void (*republish_callback)(dncp_subscriber r);

  /**
   * TLV change notification.
   *
   * This is called whenever one TLV within one node in DNCP
   * changes. This INCLUDE also the local node itself.
   *
   * @param n The node for which change notification occurs.
   * @param tlv The TLV that is being added or removed (there is no 'update').
   * @param add Flag which indicates whether the operation was add or remove.
   */
  void (*tlv_change_callback)(dncp_subscriber s,
                              dncp_node n, struct tlv_attr *tlv, bool add);

  /**
   * Node change notification.
   *
   * This is called whenever a node is being added or removed within
   * DNCP.
   *
   * @param n The node which is being added or removed.
   * @param add Flag which indicates whether the operation was add or remove.
   */
  void (*node_change_callback)(dncp_subscriber s, dncp_node n, bool add);

  /**
   * Some link-specific information changed.
   *
   * This is called whenever a link's preferred address changes, or
   * set of links itself changes.
   *
   * @param ifname The link which is being added, removed or modified.
   * @param event indicates whether the link was added, removed or updated.
   */
  void (*link_change_callback)(dncp_subscriber s, const char *ifname,
                               enum dncp_subscriber_event event);

  /**
   * TLV(s) received on a socket-notification.
   *
   * This is called whenever a message is received, either unicast or
   * multicast, on one of the socket(s) controlled by DNCP.
   *
   * NOTE: This is very low-level message handling callback; NO CHECKS
   * HAVE BEEN PERFORMED ON THE PAYLOAD (or that src/dst are really
   * allowed to send us something). Only in the (global) DTLS mode
   * handling this without address checks is probably ok, as the
   * authentication and authorization has happened before this
   * callback is called.
   */
  void (*msg_received_callback)(dncp_subscriber s,
                                const char *ifname,
                                struct sockaddr_in6 *src,
                                struct in6_addr *dst,
                                struct tlv_attr *msg);
};

/********************************************* API for handling single links */

/* (dncp_ep_i itself is implementation detail) */

typedef struct dncp_ep_struct dncp_ep_s, *dncp_ep;
struct dncp_ep_struct {
  char ifname[IFNAMSIZ]; /* Name of the endpoint. */
  /* NOTE: This MUST NOT be changed. It is kept around just for
   * usability reasons. */

  char dnsname[DNS_MAX_ESCAPED_LEN]; /* DNS FQDN or label */

  /* Trickle conf */
  hnetd_time_t trickle_imin, trickle_imax;
  int trickle_k;

  /* How frequently (overriding Trickle) we MUST send something on the
   * link. */
  hnetd_time_t keepalive_interval;
};

/**
 * Find or create a new dncp_ep_s that matches the interface.
 */
dncp_ep dncp_ep_find_by_name(dncp o, const char *name);

/**
 * Does the current DNCP instance have highest ID on the given endpoint?
 */
bool dncp_ep_has_highest_id(dncp_ep ep);

/************************************************ API for whole dncp instance */

/**
 * Create DNCP instance.
 *
 * This call will create the dncp object, and register it to uloop. In
 * case of error, NULL is returned.
 */
dncp dncp_create(void *userdata);

/**
 * Destroy DNCP instance
 *
 * This call will destroy the previous created DNCP object.
 */
void dncp_destroy(dncp o);

/**
 * Get first DNCP node.
 */
dncp_node dncp_get_first_node(dncp o);

/**
 * Publish a single TLV.
 *
 * @return The newly allocated TLV, which is valid until
 * dncp_remove_tlv is called for it. Otherwise NULL.
 */
dncp_tlv dncp_add_tlv(dncp o,
                      uint16_t type, void *data, uint16_t len,
                      int extra_bytes);

#define dncp_add_tlv_attr(o, a, bytes)                          \
  dncp_add_tlv(o, tlv_id(a), tlv_data(a), tlv_len(a), bytes)

/**
 * Find TLV with exact match (if any).
 */
dncp_tlv dncp_find_tlv(dncp o, uint16_t type, void *data, uint16_t len);

/**
 * Stop publishing a TLV.
 */
void dncp_remove_tlv(dncp o, dncp_tlv tlv);

#define dncp_remove_tlv_matching(o, t, d, dlen)         \
  dncp_remove_tlv(o, dncp_find_tlv(o, t, d, dlen))

/**
 * Remove all TLVs of particular type.
 *
 * @return The number of TLVs removed.
 */
int dncp_remove_tlvs_by_type(dncp o, int type);

/**
 * Subscribe to DNCP state change events.
 *
 * This call will register the caller as subscriber to DNCP state
 * changes. It will also trigger a series of add notifications for
 * existing state.
 */
void dncp_subscribe(dncp o, dncp_subscriber s);

/**
 * Unsubscribe from DNCP state change events.
 *
 * Inverse of dncp_subscribe (including calls to pretend to remove all
 * state).
 */
void dncp_unsubscribe(dncp o, dncp_subscriber s);

/**
 * Run DNCP state machine once. It should re-queue itself when needed.
 * (This should be mainly called from timeout callback, or from unit
 * tests).
 */
void dncp_run(dncp o);

/**
 * Poll the i/o system once. This should be called from event loop
 * whenever the udp socket has inputs.
 */
void dncp_poll(dncp o);

/************************************************************** Per-node API */

/**
 * Get next DNCP node (in order, from DNCP).
 */
dncp_node dncp_node_get_next(dncp_node n);

/**
 * Check if the DNCP node is ourselves (may require different handling).
 */
bool dncp_node_is_self(dncp_node n);

/**
 * Get the TLVs for particular DNCP node.
 */
struct tlv_attr *dncp_node_get_tlvs(dncp_node n);

#define dncp_for_each_node(o, n)                                        \
  for (n = dncp_get_first_node(o) ; n ; n = dncp_node_get_next(n))

#define dncp_node_for_each_tlv(n, a)            \
  tlv_for_each_attr(a, dncp_node_get_tlvs(n))

/******************************************************* Per-(local) tlv API */

/**
 * Get the extra byte pointer associated with the tlv (extra_bytes).
 *
 * As length is not stored anywhere, it is up to the caller to be
 * consistent. If extra_bytes is zero, the first byte pointed by the
 * return value may already be invalid.
 */
void *dncp_tlv_get_extra(dncp_tlv tlv);
