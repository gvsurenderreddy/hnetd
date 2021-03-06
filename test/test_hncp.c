/*
 * $Id: test_dncp.c $
 *
 * Author: Markus Stenberg <mstenber@cisco.com>
 *
 * Copyright (c) 2013-2015 cisco Systems, Inc.
 *
 * Created:       Thu Nov 21 13:26:21 2013 mstenber
 * Last modified: Mon Jun 29 17:07:02 2015 mstenber
 * Edit time:     110 min
 *
 */

#include "hncp_i.h"
#include "hncp_proto.h"
#include "sput.h"
#include "smock.h"
#include "platform.h"

#include "fake_log.h"

/* Lots of stubs here, rather not put __unused all over the place. */
#pragma GCC diagnostic ignored "-Wunused-parameter"

/* Only 'internal' method we use from here; normally, it is possible
 * to get NULL tlvs right after setting, until timeout causes flush to
 * network. */
void dncp_self_flush(dncp_node n);


/* Fake structures to keep pa's default config happy. */
void iface_register_user(struct iface_user *user) {}
void iface_unregister_user(struct iface_user *user) {}

struct iface* iface_get(const char *ifname )
{
  return NULL;
}

struct iface* iface_next(struct iface *prev)
{
  return NULL;
}

void iface_all_set_dhcp_send(const void *dhcpv6_data, size_t dhcpv6_len,
                             const void *dhcp_data, size_t dhcp_len)
{
}

int iface_get_preferred_address(struct in6_addr *foo, bool v4, const char *ifname)
{
  return -1;
}

int iface_get_address(struct in6_addr *addr, bool v4, const struct in6_addr *preferred)
{
	return -1;
}

/* Quiet some warnings.. */
struct platform_rpc_method;
struct blob_attr;

int platform_rpc_register(struct platform_rpc_method *m)
{
  return 0;
}

int platform_rpc_cli(const char *method, struct blob_attr *in)
{
  return 0;
}

/**************************************************************** Test cases */

void hncp_ext(void)
{
  hncp h = hncp_create();
  dncp o = hncp_get_dncp(h);
  dncp_node n;
  bool r;
  struct tlv_buf tb;
  struct tlv_attr *t;

  sput_fail_if(!o, "create works");
  n = dncp_get_first_node(o);
  sput_fail_unless(n, "first node exists");

  sput_fail_unless(dncp_node_is_self(n), "self node");

  memset(&tb, 0, sizeof(tb));
  tlv_buf_init(&tb, 0);

  dncp_self_flush(n);
  sput_fail_unless(dncp_node_get_tlvs(n), "should have tlvs");

  tlv_put(&tb, 123, NULL, 0);

  /* Put the 123 type length = 0 TLV as TLV to hncp. */
  r = dncp_add_tlv(o, 123, NULL, 0, 0);
  sput_fail_unless(r, "dncp_add_tlv ok (should work)");

  dncp_self_flush(n);
  t = dncp_node_get_tlvs(n);
  sput_fail_unless(tlv_attr_equal(t, tb.head), "tlvs consistent");

  /* Should be able to enable it on a link. */
  hncp_set_enabled(h, "eth0", true);
  hncp_set_enabled(h, "eth1", true);
  hncp_set_enabled(h, "eth1", true);

  dncp_self_flush(n);
  t = dncp_node_get_tlvs(n);
  sput_fail_unless(tlv_attr_equal(t, tb.head), "tlvs should be same");

  hncp_set_enabled(h, "eth1", false);
  hncp_set_enabled(h, "eth1", false);

  dncp_self_flush(n);
  t = dncp_node_get_tlvs(n);
  sput_fail_unless(tlv_attr_equal(t, tb.head), "tlvs should be same");

  /* Make sure run doesn't blow things up */
  dncp_ext_timeout(o);

  /* Similarly, poll should also be nop (socket should be non-blocking). */
  dncp_ext_readable(o);

  dncp_remove_tlv_matching(o, 123, NULL, 0);

  n = dncp_node_get_next(n);
  sput_fail_unless(!n, "second node should not exist");

  hncp_destroy(h);

  tlv_buf_free(&tb);
}

#include "dncp_i.h"

void hncp_int(void)
{
  /* If we want to do bit more whitebox unit testing of the whole hncp,
   * do it here. */
  hncp_s s;
  dncp o;
  dncp_node n;
  dncp_ep ep;

  hncp_init(&s);
  o = hncp_get_dncp(&s);

  /* Make sure network hash is dirty. */
  sput_fail_unless(o->network_hash_dirty, "network hash should be dirty");

  /* Make sure we can add nodes if we feel like it. */
  dncp_node_id_s ni;
  memset(&ni, 0, sizeof(ni));

  n = dncp_find_node_by_node_id(o, &ni, false);
  sput_fail_unless(!n, "dncp_find_node_by_hash w/ create=false => none");
  n = dncp_find_node_by_node_id(o, &ni, true);
  sput_fail_unless(n, "dncp_find_node_by_hash w/ create=false => !none");
  sput_fail_unless(dncp_find_node_by_node_id(o, &ni, false), "should exist");
  sput_fail_unless(dncp_find_node_by_node_id(o, &ni, false) == n, "still same");

  n = dncp_get_first_node(o);
  sput_fail_unless(n, "dncp_get_first_node");
  n = dncp_node_get_next(n);
  sput_fail_unless(!n, "dncp_node_get_next [before prune]");

  /* Play with run; initially should increment update number */
  sput_fail_unless(o->own_node->update_number == 0, "update number ok");
  dncp_ext_timeout(o);
  sput_fail_unless(o->own_node->update_number == 1, "update number ok");

  n = dncp_get_first_node(o);
  sput_fail_unless(n, "dncp_get_first_node");
  n = dncp_node_get_next(n);
  sput_fail_unless(!n, "dncp_node_get_next [after prune]");

  /* Similarly, links */
  const char *ifn = "foo";
  ep = dncp_find_ep_by_name(o, ifn);
  sput_fail_unless(ep, "dncp_find_ep_by_name => !none");
  sput_fail_unless(dncp_find_ep_by_name(o, ifn) == ep, "still same");

  /* but on second run, no */
  dncp_ext_timeout(o);
  sput_fail_unless(o->own_node->update_number == 1, "update number ok");

  dncp_add_tlv(o, 123, NULL, 0, 0);

  /* Added TLV should trigger new update */
  dncp_ext_timeout(o);
  sput_fail_unless(o->own_node->update_number == 2, "update number ok");

  /* Adding/removing TLV should NOT trigger new update. */
  dncp_tlv t2 = dncp_add_tlv(o, 124, NULL, 0, 0);
  sput_fail_unless(t2, "dncp_add_tlv failed");

  dncp_remove_tlv(o, t2);
  dncp_ext_timeout(o);
  sput_fail_unless(o->own_node->update_number == 2, "update number ok");

  hncp_uninit(&s);
}

void hncp_hash(void)
{
  /*
   * Make sure the hash function is sane,
   * c.f. https://github.com/sbyx/hnetd/issues/40
   */
  unsigned char buf[16];
  unsigned char input[] = {
    0, 0, 1, 0x54, 0xd3, 0xf7, 0xf5, 0xf6, 0xd5, 0x26, 0xd3, 0x0d  };
  unsigned char exp_buf[16] = {
    0xb1, 0xb2, 0xaf, 0xad, 0xd1, 0xaf, 0x5f, 0xf9, 0x2b, 0x24,
    0x25, 0xc4, 0xc2, 0x44, 0xe6, 0xfc };
  hncp_s s;

  hncp_init(&s);
  dncp o = hncp_get_dncp(&s);
  dncp_ext ext = dncp_get_ext(o);

  ext->cb.hash(input, sizeof(input), buf);
  sput_fail_unless(memcmp(buf, exp_buf, DNCP_HASH_LEN(s.dncp))==0, "hash ok");

  hncp_uninit(&s);
}

int main(int argc, char **argv)
{
  setbuf(stdout, NULL); /* so that it's in sync with stderr when redirected */
  openlog("test_hncp", LOG_CONS | LOG_PERROR, LOG_DAEMON);
  sput_start_testing();
  sput_enter_suite("hncp"); /* optional */
  sput_run_test(hncp_hash);
  sput_run_test(hncp_ext);
  sput_run_test(hncp_int);
  sput_leave_suite(); /* optional */
  sput_finish_testing();
  return sput_get_return_value();
}
