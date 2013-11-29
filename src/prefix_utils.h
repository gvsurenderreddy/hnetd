/*
 * Author: Pierre Pfister
 *
 * Prefixes manipulation utilities.
 *
 */

#ifndef UTILS_H
#define UTILS_H

#include <netinet/in.h>
#include <stdbool.h>

/* Prefix structure.
 * All bits following the plen first are ignored. */
struct prefix {
	struct in6_addr prefix;
	uint8_t plen;
};

extern struct prefix ipv4_in_ipv6_prefix;
extern struct prefix ipv6_ula_prefix;
extern struct prefix ipv6_ll_prefix;
extern struct prefix ipv6_global_prefix;

/* Tests whether p1 contains p2 */
bool prefix_contains(const struct prefix *p1,
					const struct prefix *p2);

#define prefix_is_ipv4(prefix) \
	prefix_contains(&ipv4_in_ipv6_prefix, prefix)

#define prefix_is_ipv6_ula(prefix) \
	prefix_contains(&ipv6_ula_prefix, prefix)

#define prefix_is_ll(prefix) \
	prefix_contains(&ipv6_ll_prefix, prefix)

#define prefix_is_global(prefix) \
	prefix_contains(&ipv6_global_prefix, prefix)

/* Compare two prefixes according to a complete order definition.
 * Will be used in trees.
 * Returns zero if equals, positive if p1 > p2,
 * negative value otherwise.
 * A prefix with longer prefix length is always bigger.
 * When prefix length is the same, bigger prefix value is bigger. */
int prefix_cmp(const struct prefix *p1,
		const struct prefix *p2);


/* Choose a random prefix of length plen, inside p, and returns 0.
 * Or returns -1 if there is not enaugh space in p for a prefix of
 * length plen. */
int prefix_random(const struct prefix *p, struct prefix *dst,
		uint8_t plen);


/* Sets prefix's last bits to zero.
 * May be useful when printing the prefix.
 * src and dst can be the same pointer. */
void prefix_canonical(struct prefix *dst, const struct prefix *src);


/* String returned in some cases */
#define PREFIX_STRERR "*prefix_print_error*"
/* Maximum needed space to print a prefix */
#define PREFIX_MAXBUFFLEN INET6_ADDRSTRLEN + 4
/* Number of available string buffer */
#define PREFIX_PRINT_BUFF_N 4

extern char __prefix_tostring_buffers[PREFIX_PRINT_BUFF_N][PREFIX_MAXBUFFLEN];

/* Writes the prefix into the specified buffer of length dst_len.
 * Returns dst upon success and NULL if buffer size is too small.
 * Canonical means the last bits of the prefix are considered as
 * zeros. */
char *prefix_ntop(char *dst, size_t dst_len,
		const struct prefix *prefix,
		bool canonical);

/* This function behaves like prefix_ntop but the string
 * PREFIX_STRERR is returned if the buffer size it too small. */
const char *prefix_ntop_s(char *dst, size_t dst_len,
		const struct prefix *prefix,
		bool canonical);

/* This function behaves like prefix_ntop but uses one of the
 * pre-allocated buffers. Returns PREFIX_STRERR if and only if
 * n is >= PREFIX_PRINT_BUFF_N */
const char *prefix_ntop_n(const struct prefix *prefix,
		size_t buff_id,
		bool canonical);

#define PREFIX_TOSTRING(prefix, buff_number) \
	prefix_ntop_n(prefix, buff_number, true);

#endif
