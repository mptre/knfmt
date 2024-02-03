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

#include <stdint.h>

#include "libks/buffer.h"
#include "libks/compiler.h"
#include "libks/tmp.h"

#if !defined(FUZZER_AFL) && !defined(FUZZER_LLVM)
#  define FUZZER_AFL
#endif

/* Work around what seems to be a GCC UBSan bug. */
#if defined(__GNUC__) && __has_attribute(no_sanitize)
#  define NO_SANITIZE_UNDEFINED no_sanitize("undefined")
#else
#  define NO_SANITIZE_UNDEFINED
#endif

struct fuzzer_target {
	void		(*buffer_cb)(const struct buffer *, void *);
	void		(*file_cb)(const char *, void *);

	unsigned int	buffer:1,
			file:1;
};

union fuzzer_callback {
	void *(*init)(void);
	void (*teardown)(void *);
};

extern const struct fuzzer_target fuzzer_target;

#define FUZZER_INIT(func)						\
	__attribute__((used))						\
	__attribute__((section("fuzzer_callback_init")))		\
	union fuzzer_callback _fuzzer_init_impl = {.init = (func)}	\

#define FUZZER_TEARDOWN(func)						\
	__attribute__((used))						\
	__attribute__((section("fuzzer_callback_teardown")))		\
	union fuzzer_callback _fuzzer_teardown_impl = {.teardown = (func)}\

#define FUZZER_TARGET_BUFFER(func)					\
	const struct fuzzer_target fuzzer_target = {			\
		.buffer_cb	= (func),				\
		.buffer		= 1,					\
	}

#define FUZZER_TARGET_FILE(func)					\
	const struct fuzzer_target fuzzer_target = {			\
		.file_cb	= (func),				\
		.file		= 1,					\
	}

#define FUZZER_SECTION(type)						\
	__attribute__((used))						\
	__attribute__((section("fuzzer_callback_" # type)))		\
	union fuzzer_callback _fuzzer_ ## type ## _default = {0}	\

/*
 * Since init and teardown callbacks are optional, ensure respective section is
 * always present.
 */
FUZZER_SECTION(init);
FUZZER_SECTION(teardown);

__attribute__((NO_SANITIZE_UNDEFINED))
static inline void *
fuzzer_init(void)
{
	/* NOLINTBEGIN(bugprone-reserved-identifier) */

	extern union fuzzer_callback __start_fuzzer_callback_init,
				     __stop_fuzzer_callback_init;
	union fuzzer_callback *it = &__start_fuzzer_callback_init;

	for (; it != &__stop_fuzzer_callback_init; it++) {
		if (it->init != NULL)
			return it->init();
	}
	return NULL;

	/* NOLINTEND(bugprone-reserved-identifier) */
}

__attribute__((NO_SANITIZE_UNDEFINED))
static inline void
fuzzer_teardown(void *userdata)
{
	/* NOLINTBEGIN(bugprone-reserved-identifier) */

	extern union fuzzer_callback __start_fuzzer_callback_teardown,
				     __stop_fuzzer_callback_teardown;
	union fuzzer_callback *it = &__start_fuzzer_callback_teardown;

	for (; it != &__stop_fuzzer_callback_teardown; it++) {
		if (it->teardown != NULL)
			it->teardown(userdata);
	}

	/* NOLINTEND(bugprone-reserved-identifier) */
}

#if defined(FUZZER_AFL)

int
main(void)
{
	void *userdata;

	userdata = fuzzer_init();

	if (fuzzer_target.buffer) {
		struct buffer *bf;

		bf = buffer_read("/dev/stdin");
		if (bf == NULL)
			__builtin_trap();
		fuzzer_target.buffer_cb(bf, userdata);
		buffer_free(bf);
	} else if (fuzzer_target.file) {
		fuzzer_target.file_cb("/dev/stdin", userdata);
	} else {
		__builtin_trap();
	}

	fuzzer_teardown(userdata);

	return 0;
}

#elif defined(FUZZER_LLVM)

#include <limits.h>
#include <unistd.h>

extern int	LLVMFuzzerTestOneInput(const uint8_t *, size_t);
extern int	LLVMFuzzerInitialize(int *, char ***);

static void *fuzzer_llvm_userdata;

int
LLVMFuzzerTestOneInput(const uint8_t *buf, size_t buflen)
{
	if (fuzzer_target.buffer) {
		struct buffer *bf;

		bf = buffer_alloc(buflen);
		if (bf == NULL)
			__builtin_trap();
		buffer_puts(bf, (const char *)buf, buflen);
		fuzzer_target.buffer_cb(bf, fuzzer_llvm_userdata);
		buffer_free(bf);
	} else if (fuzzer_target.file) {
		char path[PATH_MAX];
		int fd;

		fd = KS_tmpfd((const char *)buf, buflen, path, sizeof(path));
		if (fd == -1)
			__builtin_trap();
		fuzzer_target.file_cb(path, fuzzer_llvm_userdata);
		close(fd);
	} else {
		__builtin_trap();
	}

	return 0;
}

int
LLVMFuzzerInitialize(int *UNUSED(argc), char ***UNUSED(argv))
{
	fuzzer_llvm_userdata = fuzzer_init();

	return 0;
}

#else
#error "unknown fuzzer"
#endif
