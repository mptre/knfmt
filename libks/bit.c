/*
 * Copyright (c) 2025 Anton Lindqvist <anton@basename.se>
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

#include "libks/bit.h"

#include <stdint.h>

#include "libks/capabilities.h"

extern uint64_t KS_extract_and_deposit_native(uint64_t, uint64_t, uint64_t);

uint64_t (*KS_extract_and_deposit)(uint64_t, uint64_t, uint64_t) =
    KS_extract_and_deposit_default;

uint64_t
KS_extract_and_deposit_default(uint64_t val, uint64_t extract_mask,
    uint64_t deposit_mask)
{
	uint64_t extracted_bits = 0;
	for (uint32_t i = 0; extract_mask != 0; extract_mask >>= 1, val >>= 1) {
		if (extract_mask & 0x1)
			extracted_bits |= (val & 0x1) << i++;
	}

	uint64_t deposited_bits = 0;
	for (uint32_t i = 0; deposit_mask != 0; deposit_mask >>= 1, i += 1) {
		if (deposit_mask & 0x1) {
			deposited_bits |= (extracted_bits & 0x1) << i;
			extracted_bits >>= 1;
		}
	}

	return deposited_bits;
}

void
KS_bit_init(void)
{
#if defined(__x86_64__)
	struct KS_x86_capabilites caps;
	if (!KS_x86_capabilites(&caps))
		return;
	if (caps.bmi >= 2 /* PEXT, PDEP */)
		KS_extract_and_deposit = KS_extract_and_deposit_native;
#endif
}
