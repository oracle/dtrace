/*
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
#include "libdtrace/dt_btf.c"
#include "libdtrace/dt_module.c"

#define CHECK(x) do { if (!(x)) { \
	fprintf(stderr, "line %d: %s (allocation %d/%d)\n", __LINE__, #x, calls, fail_at); \
	exit(1); \
} } while (0)

/* Calls to malloc/realloc/free are redirected only at link time.  Volatile
 * prevents builtin allocator knowledge from hiding the wrappers' effects. */
static volatile int tracking, calls, fail_at, failed, member_fail, raw_allocations;
static void *live[1024];
static volatile size_t nlive;

extern void *__real_malloc(size_t);
extern void *__real_calloc(size_t, size_t);
extern void *__real_realloc(void *, size_t);
extern void __real_free(void *);

static int
fail(void)
{
	if (tracking && ++calls == fail_at) {
		failed = 1;
		errno = ENOMEM;
		return 1;
	}
	return 0;
}

static void *
remember(void *p)
{
	if (p && tracking) {
		CHECK(nlive < ARRAY_SIZE(live));
		live[nlive++] = p;
	}
	return p;
}

static void
forget(void *p)
{
	size_t i;
	for (i = 0; i < nlive; i++)
		if (live[i] == p) {
			live[i] = live[--nlive];
			return;
		}
}

/* Scope failures to DTrace allocations.  libctf may call libiberty allocators
 * exported by the executable, then free their results inside the shared
 * library; those allocations are outside this ownership ledger. */
void *__wrap_malloc(size_t n)
{
	if (!raw_allocations) return __real_malloc(n);
	return fail() ? NULL : remember(__real_malloc(n));
}
void *__wrap_calloc(size_t n, size_t s)
{
	if (!raw_allocations) return __real_calloc(n, s);
	return fail() ? NULL : remember(__real_calloc(n, s));
}
extern void *__real_dt_alloc(dtrace_hdl_t *, size_t);
extern void *__real_dt_zalloc(dtrace_hdl_t *, size_t);
extern void *__real_dt_calloc(dtrace_hdl_t *, size_t, size_t);
void *__wrap_dt_alloc(dtrace_hdl_t *dt, size_t n)
{
	if (fail()) { dt_set_errno(dt, EDT_NOMEM); return NULL; }
	return remember(__real_dt_alloc(dt, n));
}
void *__wrap_dt_zalloc(dtrace_hdl_t *dt, size_t n)
{
	if (fail()) { dt_set_errno(dt, EDT_NOMEM); return NULL; }
	return remember(__real_dt_zalloc(dt, n));
}
void *__wrap_dt_calloc(dtrace_hdl_t *dt, size_t n, size_t s)
{
	if (fail()) { dt_set_errno(dt, EDT_NOMEM); return NULL; }
	return remember(__real_dt_calloc(dt, n, s));
}
void __wrap_free(void *p) { forget(p); __real_free(p); }
void *
__wrap_realloc(void *p, size_t n)
{
	void *q;
	if (!raw_allocations)
		return __real_realloc(p, n);
	if (fail())
		return NULL;
	q = __real_realloc(p, n);
	if (q || n == 0)
		forget(p);
	return remember(q);
}

#ifdef HAVE_LIBCTF
extern int __real_ctf_add_member_offset(ctf_dict_t *, ctf_id_t, const char *, ctf_id_t, unsigned long);
int
__wrap_ctf_add_member_offset(ctf_dict_t *ctf, ctf_id_t id, const char *name,
			   ctf_id_t type, unsigned long offset)
{
	if (member_fail && --member_fail == 0)
		/* Ask libctf to produce a real error after a recursive shell exists. */
		return __real_ctf_add_member_offset(ctf, CTF_ERR, name, type, offset);
	return __real_ctf_add_member_offset(ctf, id, name, type, offset);
}
#endif

static void
start(int nth)
{
	CHECK(nlive == 0);
	calls = failed = 0;
	fail_at = nth;
	tracking = 1;
}

static void
finish(void)
{
	CHECK(nlive == 0);
	tracking = 0;
}

static int
fds(void)
{
	DIR *dir = opendir("/proc/self/fd");
	struct dirent *entry;
	int count = 0;
	CHECK(dir != NULL);
	while ((entry = readdir(dir)) != NULL)
		if (entry->d_name[0] != '.')
			count++;
	closedir(dir);
	return count;
}

static void
fixture(const char *path)
{
	const uint32_t types[] = {
		1, BTF_KIND_INT << 24, 4, BTF_INT_SIGNED << 24 | 32,
		5, BTF_KIND_STRUCT << 24 | 2, 16,
		10, 3, 0, 15, 1, 64,
		0, BTF_KIND_PTR << 24, 2
	};
	const char names[] = "\0int\0node\0next\0value\0";
	btf_header_t hdr = { .magic = BTF_MAGIC, .version = BTF_VERSION,
		.hdr_len = sizeof(hdr), .type_len = sizeof(types),
		.str_off = sizeof(types), .str_len = sizeof(names) };
	FILE *fp = fopen(path, "wb");
	CHECK(fp != NULL);
	CHECK(fwrite(&hdr, sizeof(hdr), 1, fp) == 1);
	CHECK(fwrite(types, sizeof(types), 1, fp) == 1);
	CHECK(fwrite(names, sizeof(names), 1, fp) == 1);
	CHECK(fclose(fp) == 0);
}

static int
load(const char *path, int nth)
{
	dtrace_hdl_t dt = { 0 };
	dt_btf_t *btf;
	int count, before = fds();

	start(nth);
	btf = dt_btf_load_file(&dt, path, NULL);
	count = calls;
	if (nth) {
		CHECK(failed && btf == NULL && dt.dt_btferr == ENOMEM);
		CHECK(nlive == 0);
		fail_at = 0;
		btf = dt_btf_load_file(&dt, path, NULL);
	}
	CHECK(btf && btf->type_cnt == 4);
	dt_btf_destroy(&dt, btf);
	finish();
	CHECK(fds() == before);
	return count;
}

#ifdef HAVE_LIBCTF
static int
convert(const char *path, int nth, int bad_member)
{
	dtrace_hdl_t dt = { 0 };
	dt_module_t kernel = { .dm_name = "vmlinux" };
	dt_module_t *shared = dt_module_create(&dt, "shared_ctf");
	dt_btf_t *btf = dt_btf_load_file(&dt, path, NULL);
	ctf_dict_t *ctf;
	ctf_id_t node;
	ctf_membinfo_t member;
	int count;
	uint32_t i;

	CHECK(shared && btf);
	dt.dt_exec = &kernel;
	dt.dt_conf.dtc_ctfmodel = CTF_MODEL_LP64;
	kernel.dm_btf = btf;
	start(nth);
	ctf = dt_btf_to_ctf(&dt, NULL, btf);
	count = calls;
	if (nth) {
		CHECK(failed && ctf == NULL && dtrace_errno(&dt) == EDT_NOMEM);
		CHECK(btf->ctfids == NULL && btf->ctf_state == NULL && btf->ctf_pending == NULL);
		CHECK(nlive == 0);
		fail_at = 0;
		ctf = dt_btf_to_ctf(&dt, NULL, btf);
	}
	CHECK(ctf != NULL);
	shared->dm_ctfp = ctf;
	if (bad_member) {
		member_fail = bad_member;
		CHECK(dt_ctf_lookup_by_name(&dt, ctf, "struct node") == CTF_ERR);
		CHECK(member_fail == 0 && btf->ctf_pending_cnt == 0);
		for (i = 1; i < btf->type_cnt; i++) {
			CHECK(btf->ctfids[i] == CTF_ERR);
			CHECK(btf->ctf_state[i] == DT_BTF_CTF_IDLE);
		}
		CHECK(ctf_lookup_by_name(ctf, "struct node") == CTF_ERR);
	}
	node = dt_ctf_lookup_by_name(&dt, ctf, "struct node");
	CHECK(node != CTF_ERR && ctf_type_size(ctf, node) == 16);
	CHECK(ctf_member_info(ctf, node, "next", &member) == 0);
	CHECK(ctf_type_reference(ctf, member.ctm_type) == node);
	CHECK(ctf_member_info(ctf, node, "value", &member) == 0 && member.ctm_offset == 64);
	ctf_close(ctf);
	shared->dm_ctfp = NULL;
	dt_btf_destroy(&dt, btf);
	finish();
	dt_htab_destroy(dt.dt_mods);
	return count;
}
#endif

static void
ranges(void)
{
	dtrace_hdl_t dt = { 0 };
	dt_module_t module = { .dm_dtp = &dt };
	int text;

	dt_list_append(&dt.dt_modlist, &module);
	raw_allocations = 1;
	start(0);
	for (text = 0; text < 2; text++) {
		dtrace_addr_range_t *range, *saved;
		size_t i;

		fail_at = calls + 1;
		failed = 0;
		CHECK(dtrace_addr_range_grow(&module, text) == NULL && failed);
		fail_at = 0;
		for (i = 0; i < 4; i++) {
			CHECK((range = dtrace_addr_range_grow(&module, text)) != NULL);
			range->dar_va = 0x1000 + text * 0x1000 + i * 0x100;
			range->dar_size = 0x10;
		}
		saved = text ? module.dm_text_addrs : module.dm_data_addrs;
		fail_at = calls + 1;
		failed = 0;
		CHECK(dtrace_addr_range_grow(&module, text) == NULL && failed);
		CHECK((text ? module.dm_text_addrs : module.dm_data_addrs) == saved);
		CHECK((text ? module.dm_text_addrs_size : module.dm_data_addrs_size) == 4);
		CHECK((text ? module.dm_text_addrs_cap : module.dm_data_addrs_cap) == 4);
		CHECK(saved[3].dar_va == 0x1300 + text * 0x1000);
		fail_at = 0;
		CHECK(dtrace_addr_range_grow(&module, text) != NULL);
		/* Exclude the new empty range from the sorted lookup fixture. */
		if (text) module.dm_text_addrs_size--; else module.dm_data_addrs_size--;
		/* The range remains owned until the end of this test. */
		CHECK(nlive == (size_t)text + 1);
	}
	fail_at = calls + 1;
	failed = 0;
	CHECK(dt_module_by_addr(&dt, 0x1005) == &module && failed);
	CHECK(!dt.dt_modranges_valid && dt.dt_modranges == NULL);
	fail_at = 0;
	CHECK(dt_module_by_addr(&dt, 0x2005) == &module);
	CHECK(dt.dt_modranges_valid && dt.dt_modranges_count == 8);
	CHECK(dt_module_by_addr(&dt, 0x1010) == NULL);
	dt_module_ranges_invalidate(&dt);
	free(module.dm_text_addrs);
	free(module.dm_data_addrs);
	finish();
	raw_allocations = 0;
}

int
main(int argc, char **argv)
{
	int i, count;
	CHECK(argc == 2);
	fixture(argv[1]);
	count = load(argv[1], 0);
	CHECK(count >= 5);
	for (i = 1; i <= count; i++)
		load(argv[1], i);
#ifdef HAVE_LIBCTF
	count = convert(argv[1], 0, 0);
	CHECK(count >= 3);
	for (i = 1; i <= count; i++)
		convert(argv[1], i, 0);
	convert(argv[1], 0, 1);
	convert(argv[1], 0, 2);
#endif
	ranges();
	return 0;
}
