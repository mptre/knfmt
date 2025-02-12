/*
 * Copyright (c) 2023 Anton Lindqvist <anton@basename.se>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "libks/map.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "libks/arithmetic.h"
#include "libks/compiler.h"

struct map_element {
	struct map_element *prev;	/* prev element in app order      */
	struct map_element *next;	/* next element in app order      */
	struct map_element *hh_prev;	/* previous hh in bucket order    */
	struct map_element *hh_next;	/* next hh in bucket order        */
	const void *key;		/* ptr to enclosing struct's key  */
	size_t keylen;			/* enclosing struct's key len     */
	unsigned int hashv;		/* result of hash-fcn(key)        */
};

struct map {
	struct UT_hash_table	*table;
	struct map_element	*head;

	struct {
		size_t	size;
	} key;

	struct {
		size_t	size;
	} element;

	struct {
		size_t	size;
		size_t	size_aligned;
	} val;

	unsigned int		 flags;
};

struct hash_key {
	union {
		const uint8_t	*u8;
		const uint32_t	*u32;
		const uintptr_t	 ptr;
	} u;
	size_t	len;
};

static struct map_element	*map_alloc_element(struct map *, size_t);

static void	*element_get_key(const struct map *, struct map_element *);
static void	*element_get_val(struct map_element *);

static const void	*key_get_ptr(const struct map *, const void *const *);
static size_t		 key_get_size(const struct map *, const void *);

static int	pointer_align(size_t, size_t *);

static int			 HASH_ADD(struct map *, const void *,
    size_t, struct map_element *);
static int			 HASH_ADD_TO_TABLE(struct map *,
    unsigned int, struct map_element *);
static void			 HASH_APPEND_LIST(struct map *,
    struct map_element *);
static void			 HASH_DELETE(struct map *,
    struct map_element *);
static void			 HASH_DEL_IN_BKT(struct map *,
    const struct map_element *);
static int			 HASH_EXPAND_BUCKETS(struct map *);
static struct map_element	*HASH_FIND(struct map *, const void *,
    size_t);
static struct UT_hash_table	*HASH_MAKE_TABLE(struct map_element *);
static unsigned			 HASH_TO_BKT(unsigned int, unsigned int);
static unsigned			 HASH_JEN(const void *, size_t)
	__attribute__((NO_SANITIZE_UNSIGNED_INTEGER_OVERFLOW));

int
map_init(void **mp, size_t keysize, size_t valsize, unsigned int flags)
{
	struct map *m;
	size_t elementsize, valsize_aligned;

	if (pointer_align(valsize, &valsize_aligned))
		goto overflow;
	if (KS_size_add_overflow(valsize_aligned, sizeof(struct map_element),
	    &elementsize))
		goto overflow;
	if ((flags & MAP_KEY_STR) == 0 &&
	    KS_size_add_overflow(keysize, elementsize, &elementsize))
		goto overflow;

	m = calloc(1, sizeof(*m));
	if (m == NULL)
		return 1;
	m->key.size = keysize;
	m->val.size = valsize;
	m->val.size_aligned = valsize_aligned;
	m->element.size = elementsize;
	m->flags = flags;
	*mp = m;
	return 0;

overflow:
	errno = EOVERFLOW;
	return 1;
}

void
map_free(void *mp)
{
	struct map *m = mp;
	struct map_element *el, *nx;

	if (m == NULL)
		return;

	for (el = m->head; el != NULL; el = nx) {
		nx = el->next;
		HASH_DELETE(m, el);
	}
	free(m);
}

void *
map_insert(void *mp, const void *const *key)
{
	struct map *m = mp;

	return map_insert_n(mp, key, key_get_size(m, key_get_ptr(m, key)));
}

void *
map_insert_n(void *mp, const void *const *key, size_t keysize)
{
	struct map *m = mp;
	struct map_element *el;
	const void *keyptr;

	keyptr = key_get_ptr(m, key);
	el = map_alloc_element(m, keysize);
	if (el == NULL)
		return NULL;
	memcpy(element_get_key(m, el), keyptr, keysize);
	if (HASH_ADD(m, element_get_key(m, el), keysize, el))
		return NULL;
	return element_get_val(el);
}

void *
map_key(void *mp, void *val)
{
	struct map *m = mp;
	struct map_element *el = (struct map_element *)val;

	return element_get_key(m, &el[-1]);
}

void *
map_find(void *mp, const void *const *key)
{
	struct map *m = mp;
	struct map_element *el;
	const void *keyptr;
	size_t keysize;

	keyptr = key_get_ptr(m, key);
	keysize = key_get_size(m, keyptr);
	el = HASH_FIND(m, keyptr, keysize);
	return el != NULL ? element_get_val(el) : NULL;
}

void *
map_find_n(void *mp, const void *const *key, size_t keysize)
{
	struct map *m = mp;
	struct map_element *el;
	const void *keyptr;

	keyptr = key_get_ptr(m, key);
	el = HASH_FIND(m, keyptr, keysize);
	return el != NULL ? element_get_val(el) : NULL;
}

void
map_remove(void *mp, const void *const *key)
{
	struct map *m = mp;
	struct map_element *el;
	const void *keyptr;
	size_t keysize;

	keyptr = key_get_ptr(m, key);
	keysize = key_get_size(m, keyptr);
	el = HASH_FIND(m, keyptr, keysize);
	if (el == NULL)
		return;
	HASH_DELETE(m, el);
}

void *
map_iterate(void *mp, struct map_iterator *it)
{
	struct map *m = mp;
	struct map_element *el;

	if (it->el == NULL && it->nx == NULL) {
		el = m->head;
		if (el == NULL)
			return NULL;
		it->el = el;
		it->nx = el->next;
		return element_get_val(el);
	}

	el = it->nx;
	if (el == NULL)
		return NULL;
	it->nx = el->next;
	return element_get_val(el);
}

static struct map_element *
map_alloc_element(struct map *m, size_t keysize)
{
	struct map_element *el;
	size_t totsize = m->element.size;

	if (m->flags & MAP_KEY_STR) {
		size_t keysize_aligned;

		/* Add one byte to ensure NUL-terminator. */
		if (pointer_align(keysize, &keysize_aligned) ||
		    KS_size_add_overflow(keysize_aligned, totsize, &totsize) ||
		    KS_size_add_overflow(1, totsize, &totsize)) {
			errno = EOVERFLOW;
			return NULL;
		}
	}

	el = calloc(1, totsize);
	if (el == NULL)
		return NULL;
	return el;
}

static void *
element_get_key(const struct map *m, struct map_element *el)
{
	return (uint8_t *)&el[1] + m->val.size_aligned;
}

static void *
element_get_val(struct map_element *el)
{
	return &el[1];
}

static const void *
key_get_ptr(const struct map *m, const void *const *key)
{
	return (m->flags & MAP_KEY_PTR) ? *key : (const void *)key;
}

static size_t
key_get_size(const struct map *m, const void *key)
{
	return (m->flags & MAP_KEY_STR) ?
	    strlen((const char *)key) : m->key.size;
}

static int
pointer_align(size_t size, size_t *out)
{
	size_t pointer_size = sizeof(void *);
	size_t sum;

	if (KS_size_add_overflow(size, pointer_size - 1, &sum))
		return 1;
	*out = sum & ~(pointer_size - 1);
	return 0;
}

/*
 * Copyright (c) 2003-2022, Troy D. Hanson  https://troydhanson.github.io/uthash/
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* initial number of buckets */
#define HASH_INITIAL_NUM_BUCKETS 32U     /* initial number of buckets        */
#define HASH_INITIAL_NUM_BUCKETS_LOG2 5U /* lg2 of initial number of buckets */
#define HASH_BKT_CAPACITY_THRESH 10U     /* expand when bucket count reaches */

struct UT_hash_bucket {
	struct map_element *hh_head;
	unsigned int count;

	/* expand_mult is normally set to 0. In this situation, the max chain length
	* threshold is enforced at its default value, HASH_BKT_CAPACITY_THRESH. (If
	* the bucket's chain exceeds this length, bucket expansion is triggered).
	* However, setting expand_mult to a non-zero value delays bucket expansion
	* (that would be triggered by additions to this particular bucket)
	* until its chain length reaches a *multiple* of HASH_BKT_CAPACITY_THRESH.
	* (The multiplier is simply expand_mult+1). The whole idea of this
	* multiplier is to reduce bucket expansions, since they are expensive, in
	* situations where we know that a particular bucket tends to be overused.
	* It is better to let its chain length grow to a longer yet-still-bounded
	* value, than to do an O(n) bucket expansion too often.
	*/
	unsigned expand_mult;
};

struct UT_hash_table {
	struct UT_hash_bucket *buckets;
	unsigned int num_buckets, log2_num_buckets;
	unsigned int num_items;
	struct map_element *tail; /* tail hh in app order, for fast append    */

	/* in an ideal situation (all buckets used equally), no bucket would have
	 * more than ceil(#items/#buckets) items. that's the ideal chain length. */
	unsigned ideal_chain_maxlen;

	/* nonideal_items is the number of items in the hash whose chain position
	 * exceeds the ideal chain maxlen. these items pay the penalty for an uneven
	 * hash distribution; reaching them in a chain traversal takes >ideal steps */
	unsigned nonideal_items;

	/* ineffective expands occur when a bucket doubling was performed, but
	 * afterward, more than half the items in the hash had nonideal chain
	 * positions. If this happens on two consecutive expansions we inhibit any
	 * further expansion, as it's not helping; this happens when the hash
	 * function isn't a good fit for the key domain. When expansion is inhibited
	 * the hash will still work, albeit no longer in constant time. */
	unsigned ineff_expands, noexpand;
};

static int
HASH_ADD(struct map *m, const void *key, size_t keylen,
    struct map_element *add)
{
	unsigned int _ha_hashv;

	_ha_hashv = HASH_JEN(key, keylen);
	add->hashv = _ha_hashv;
	add->key = key;
	add->keylen = keylen;
	if (m->head == NULL) {
		m->table = HASH_MAKE_TABLE(add);
		if (m->table == NULL)
			return 1;
		m->head = add;
	} else {
		HASH_APPEND_LIST(m, add);
	}
	return HASH_ADD_TO_TABLE(m, _ha_hashv, add);
}

static int
HASH_ADD_TO_TABLE(struct map *m, unsigned int hashval, struct map_element *add)
{
	struct UT_hash_bucket *bkt;
	unsigned int bkt_idx;

	m->table->num_items++;
	bkt_idx = HASH_TO_BKT(hashval, m->table->num_buckets);
	bkt = &m->table->buckets[bkt_idx];
	bkt->count++;
	add->hh_next = bkt->hh_head;
	add->hh_prev = NULL;
	if (bkt->hh_head != NULL)
		bkt->hh_head->hh_prev = add;
	bkt->hh_head = add;
	if ((bkt->count >=
	    (bkt->expand_mult + 1U) * HASH_BKT_CAPACITY_THRESH) &&
	    !m->table->noexpand) {
		if (HASH_EXPAND_BUCKETS(m))
			return 1;
	}
	return 0;
}

static void
HASH_APPEND_LIST(struct map *m, struct map_element *add)
{
	add->next = NULL;
	add->prev = m->table->tail;
	m->table->tail->next = add;
	m->table->tail = add;
}

/* delete "delptr" from the hash table.
 * "the usual" patch-up process for the app-order doubly-linked-list.
 * The use of _hd_hh_del below deserves special explanation.
 * These used to be expressed using (delptr) but that led to a bug
 * if someone used the same symbol for the head and deletee, like
 *  HASH_DELETE(hh,users,users);
 * We want that to work, but by changing the head (users) below
 * we were forfeiting our ability to further refer to the deletee (users)
 * in the patch-up process. Solution: use scratch space to
 * copy the deletee pointer, then the latter references are via that
 * scratch pointer rather than through the repointed (users) symbol.
 */
static void
HASH_DELETE(struct map *m, struct map_element *del)
{
	const struct map_element *_hd_hh_del = del;
	struct UT_hash_table *tbl = m->table;

	if (_hd_hh_del->prev == NULL && _hd_hh_del->next == NULL) {
		free(tbl->buckets);
		free(tbl);
		m->head = NULL;
	} else {
		if (_hd_hh_del == tbl->tail)
			tbl->tail = _hd_hh_del->prev;
		if (_hd_hh_del->prev != NULL)
			_hd_hh_del->prev->next = _hd_hh_del->next;
		else
			m->head = _hd_hh_del->next;
		if (_hd_hh_del->next != NULL)
			_hd_hh_del->next->prev = _hd_hh_del->prev;
		HASH_DEL_IN_BKT(m, _hd_hh_del);
		tbl->num_items--;
	}

	free(del);
}

static void
HASH_DEL_IN_BKT(struct map *m, const struct map_element *del)
{
	struct UT_hash_bucket *bkt;
	unsigned int bkt_idx;

	bkt_idx = HASH_TO_BKT(del->hashv, m->table->num_buckets);
	bkt = &m->table->buckets[bkt_idx];
	bkt->count--;
	if (bkt->hh_head == del)
		bkt->hh_head = del->hh_next;
	if (del->hh_prev)
		del->hh_prev->hh_next = del->hh_next;
	if (del->hh_next)
		del->hh_next->hh_prev = del->hh_prev;
}

/* Bucket expansion has the effect of doubling the number of buckets
 * and redistributing the items into the new buckets. Ideally the
 * items will distribute more or less evenly into the new buckets
 * (the extent to which this is true is a measure of the quality of
 * the hash function as it applies to the key domain).
 *
 * With the items distributed into more buckets, the chain length
 * (item count) in each bucket is reduced. Thus by expanding buckets
 * the hash keeps a bound on the chain length. This bounded chain
 * length is the essence of how a hash provides constant time lookup.
 *
 * The calculation of tbl->ideal_chain_maxlen below deserves some
 * explanation. First, keep in mind that we're calculating the ideal
 * maximum chain length based on the *new* (doubled) bucket count.
 * In fractions this is just n/b (n=number of items,b=new num buckets).
 * Since the ideal chain length is an integer, we want to calculate
 * ceil(n/b). We don't depend on floating point arithmetic in this
 * hash, so to calculate ceil(n/b) with integers we could write
 *
 *      ceil(n/b) = (n/b) + ((n%b)?1:0)
 *
 * and in fact a previous version of this hash did just that.
 * But now we have improved things a bit by recognizing that b is
 * always a power of two. We keep its base 2 log handy (call it lb),
 * so now we can write this with a bit shift and logical AND:
 *
 *      ceil(n/b) = (n>>lb) + ( (n & (b-1)) ? 1:0)
 *
 */
static int
HASH_EXPAND_BUCKETS(struct map *m)
{
	struct UT_hash_table *tbl = m->table;
	struct UT_hash_bucket *_he_newbkt, *newbuckets;
	struct map_element *_he_hh_nxt, *_he_thh;
	unsigned int bkt_idx, i, nbuckets;

	if (KS_u32_mul_overflow(tbl->num_buckets, 2, &nbuckets))
		return 1;
	newbuckets = calloc(nbuckets, sizeof(struct UT_hash_bucket));
	if (newbuckets == NULL)
		return 1;

	tbl->ideal_chain_maxlen =
	    (tbl->num_items >> (tbl->log2_num_buckets + 1U)) +
	    (((tbl->num_items & ((tbl->num_buckets*2U) - 1U)) !=
	      0U) ? 1U : 0U);
	tbl->nonideal_items = 0;
	for (i = 0; i < tbl->num_buckets; i++) {
		_he_thh = tbl->buckets[i].hh_head;
		while (_he_thh != NULL) {
			_he_hh_nxt = _he_thh->hh_next;
			bkt_idx = HASH_TO_BKT(_he_thh->hashv, nbuckets);
			_he_newbkt = &newbuckets[bkt_idx];
			if (++(_he_newbkt->count) > tbl->ideal_chain_maxlen) {
				tbl->nonideal_items++;
				if (_he_newbkt->count >
				    _he_newbkt->expand_mult *
				    tbl->ideal_chain_maxlen)
					_he_newbkt->expand_mult++;
			}
			_he_thh->hh_prev = NULL;
			_he_thh->hh_next = _he_newbkt->hh_head;
			if (_he_newbkt->hh_head != NULL)
				_he_newbkt->hh_head->hh_prev = _he_thh;
			_he_newbkt->hh_head = _he_thh;
			_he_thh = _he_hh_nxt;
		}
	}
	free(tbl->buckets);
	tbl->num_buckets *= 2U;
	tbl->log2_num_buckets++;
	tbl->buckets = newbuckets;
	tbl->ineff_expands = (tbl->nonideal_items >
	    (tbl->num_items >> 1)) ? (tbl->ineff_expands + 1U) : 0U;
	if (tbl->ineff_expands > 1U)
		tbl->noexpand = 1;
	return 0;
}

static struct map_element *
HASH_FIND(struct map *m, const void *key, size_t keylen)
{
	struct map_element *el;
	unsigned int bkt_idx, hashv;

	if (m->head == NULL)
		return NULL;

	hashv = HASH_JEN(key, keylen);
	bkt_idx = HASH_TO_BKT(hashv, m->table->num_buckets);
	el = m->table->buckets[bkt_idx].hh_head;
	while (el != NULL) {
		if (el->hashv == hashv && el->keylen == keylen &&
		    memcmp(el->key, key, keylen) == 0)
			return el;
		el = el->hh_next;
	}
	return NULL;
}

static struct UT_hash_table *
HASH_MAKE_TABLE(struct map_element *tail)
{
	struct UT_hash_table *tbl;

	tbl = calloc(1, sizeof(*tbl));
	if (tbl == NULL)
		return NULL;
	tbl->tail = tail;
	tbl->num_buckets = HASH_INITIAL_NUM_BUCKETS;
	tbl->log2_num_buckets = HASH_INITIAL_NUM_BUCKETS_LOG2;
	tbl->buckets = calloc(HASH_INITIAL_NUM_BUCKETS,
	    sizeof(struct UT_hash_bucket));
	if (tbl->buckets == NULL) {
		free(tbl);
		return NULL;
	}
	return tbl;
}

static unsigned
HASH_TO_BKT(unsigned int hashv, unsigned int num_bkts)
{
	return hashv & (num_bkts - 1U);
}

#define HASH_JEN_MIX(a, b, c)						\
do {									\
  (a) -= (b); (a) -= (c); (a) ^= (c) >> 13;				\
  (b) -= (c); (b) -= (a); (b) ^= (a) << 8;				\
  (c) -= (a); (c) -= (b); (c) ^= (b) >> 13;				\
  (a) -= (b); (a) -= (c); (a) ^= (c) >> 12;				\
  (b) -= (c); (b) -= (a); (b) ^= (a) << 16;				\
  (c) -= (a); (c) -= (b); (c) ^= (b) >> 5;				\
  (a) -= (b); (a) -= (c); (a) ^= (c) >> 3;				\
  (b) -= (c); (b) -= (a); (b) ^= (a) << 10;				\
  (c) -= (a); (c) -= (b); (c) ^= (b) >> 15;				\
} while (0)

static void
hash_key_init(struct hash_key *key, const void *buf, size_t buflen)
{
	key->u.u8 = buf;
	key->len = buflen;
}

static uint32_t
hash_key_read_u32(struct hash_key *key)
{
	uint32_t val;
	unsigned int size = sizeof(val);

	if (unlikely((key->u.ptr & (size - 1)) != 0))
		memcpy(&val, key->u.u8, 4);
	else
		val = key->u.u32[0];
	key->u.u8 += size;
	key->len -= size;

	return val;
}

static unsigned
HASH_JEN(const void *key, size_t keylen)
{
	struct hash_key _hj_key;
	size_t k;
	unsigned int hashv = 0xfeedbeefu;
	unsigned int _hj_i, _hj_j;

	hash_key_init(&_hj_key, key, keylen);

	_hj_i = _hj_j = 0x9e3779b9u;
	k = keylen;
	while (k >= 12U) {
		_hj_i += hash_key_read_u32(&_hj_key);
		_hj_j += hash_key_read_u32(&_hj_key);
		hashv += hash_key_read_u32(&_hj_key);

		HASH_JEN_MIX(_hj_i, _hj_j, hashv);

		k -= 12U;
	}
	hashv += (unsigned int)(keylen);
	switch (k) {
	case 11:
		hashv += ((unsigned int)_hj_key.u.u8[10] << 24);
		FALLTHROUGH;
	case 10:
		hashv += ((unsigned int)_hj_key.u.u8[9] << 16);
		FALLTHROUGH;
	case 9:
		hashv += ((unsigned int)_hj_key.u.u8[8] << 8);
		FALLTHROUGH;
	case 8:
		_hj_j += ((unsigned int)_hj_key.u.u8[7] << 24);
		FALLTHROUGH;
	case 7:
		_hj_j += ((unsigned int)_hj_key.u.u8[6] << 16);
		FALLTHROUGH;
	case 6:
		_hj_j += ((unsigned int)_hj_key.u.u8[5] << 8);
		FALLTHROUGH;
	case 5:
		_hj_j += _hj_key.u.u8[4];
		FALLTHROUGH;
	case 4:
		_hj_i += ((unsigned int)_hj_key.u.u8[3] << 24);
		FALLTHROUGH;
	case 3:
		_hj_i += ((unsigned int)_hj_key.u.u8[2] << 16);
		FALLTHROUGH;
	case 2:
		_hj_i += ((unsigned int)_hj_key.u.u8[1] << 8);
		FALLTHROUGH;
	case 1:
		_hj_i += _hj_key.u.u8[0];
		FALLTHROUGH;
	default:
		break;
	}
	HASH_JEN_MIX(_hj_i, _hj_j, hashv);
	return hashv;
}
