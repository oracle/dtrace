/*
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <dt_bpf.h>
#include <dt_module.h>
#include <dt_symtab.h>

#define CHECK(expr) do { \
	if (!(expr)) { \
		fprintf(stderr, "Check failed at line %d: %s\n", __LINE__, #expr); \
		return 1; \
	} \
} while (0)

static const char *symbols;
static int kallsyms;
static uint32_t btf_generation, btf_id;

extern FILE *__real_fopen64(const char *, const char *);

/* Supply changing kernel symbol snapshots without loading real modules. */
FILE *
__wrap_fopen64(const char *path, const char *mode)
{
	if (symbols != NULL) {
		if (strcmp(path, "/proc/kallmodsyms") == 0) {
			if (!kallsyms)
				return fmemopen((void *)symbols, strlen(symbols), "r");
			errno = ENOENT;
			return NULL;
		}
		if (strcmp(path, "/proc/kallsyms") == 0)
			return fmemopen((void *)symbols, strlen(symbols), "r");
	}
	return __real_fopen64(path, mode);
}

extern int __real_dt_bpf_btf_get_fd_by_id(uint32_t);
extern int __real_dt_bpf_btf_get_info_by_fd(int, btf_info_t *, uint32_t *);

int
__wrap_dt_bpf_btf_get_next_id(uint32_t curr, uint32_t *next)
{
	/*
	 * Do not cache real kernel IDs during dtrace_open(): they can collide
	 * with the synthetic IDs used by subsequent discovery snapshots.
	 */
	if (symbols == NULL || curr == btf_generation + 3) {
		errno = ENOENT;
		return -1;
	}
	*next = curr == 0 ? btf_generation + 1 : curr + 1;
	return 0;
}

int
__wrap_dt_bpf_btf_get_fd_by_id(uint32_t id)
{
	if (symbols == NULL)
		return __real_dt_bpf_btf_get_fd_by_id(id);
	btf_id = id;
	return open("/dev/null", O_RDONLY);
}

int
__wrap_dt_bpf_btf_get_info_by_fd(int fd, btf_info_t *info, uint32_t *size)
{
	static const char *names[] = {
		"discovery_first", "discovery_second", "discovery_offline"
	};

	if (symbols == NULL)
		return __real_dt_bpf_btf_get_info_by_fd(fd, info, size);
	snprintf((char *)(uintptr_t)info->name, info->name_len, "%s",
		 names[btf_id - btf_generation - 1]);
	return 0;
}

static int
check_discovery(int fallback)
{
	dtrace_hdl_t *dtp;
	dt_module_t *kernel, *first, *second, *offline;
	dt_symtab_t *kernel_syms, *first_syms;
	dt_symbol_t *sym, *entry;
	int count, found_first, found_second;
	dtrace_addr_range_t *ranges, *kernel_ranges;
	ctf_file_t *types;
	GElf_Sym elf;
	int err, i;

	CHECK((dtp = dtrace_open(DTRACE_VERSION, DTRACE_O_LP64, &err)) != NULL);
	kernel = dtp->dt_exec;
	CHECK(kernel != NULL);
	kernel_syms = kernel->dm_kernsyms;
	kernel_ranges = kernel->dm_text_addrs;
	kallsyms = fallback;
	btf_generation = 100;

	/* A module can be known through its types before it is discovered live. */
	CHECK((offline = dt_module_create(dtp, "discovery_offline")) != NULL);
	CHECK((types = ctf_create(&err)) != NULL);
	offline->dm_ctfp = types;
	offline->dm_flags |= DT_DM_KERNEL | DT_DM_KERN_UNLOADED;

	symbols = fallback ?
	    "100 T _end\n"
	    "1000 T discovery_symbol [discovery_first]\n"
	    "1010 T discovery_tail [discovery_first]\n" :
	    "100 1 T _end\n"
	    "1000 10 T discovery_symbol [discovery_first]\n"
	    "1010 10 T discovery_tail [discovery_first]\n";
	CHECK(dtrace_update(dtp) == 0);
	CHECK((first = dt_module_lookup_by_name(dtp, "discovery_first")) != NULL);
	CHECK(first->dm_flags & DT_DM_KERNEL);
	CHECK(first->dm_btf_id == 101);
	CHECK((sym = dt_module_symbol_by_name(dtp, first, "discovery_symbol")) != NULL);
	first_syms = first->dm_kernsyms;
	ranges = first->dm_text_addrs;
	CHECK(first->dm_text_addrs_size == 1);
	CHECK(ranges->dar_va == 0x1000);
	CHECK(ranges->dar_size == (fallback ? 0x11 : 0x20));
	CHECK(dt_symbol_by_addr(first_syms, 0x1000) == sym);

	/* Keep the old module while discovering another, including a duplicate name. */
	btf_generation = 200;
	symbols = fallback ?
	    "100 T _end\n"
	    "1000 T discovery_symbol [discovery_first]\n"
	    "1010 T discovery_tail [discovery_first]\n"
	    "2000 T discovery_symbol [discovery_second]\n"
	    "3000 T discovery_live [discovery_offline]\n" :
	    "100 1 T _end\n"
	    "1000 10 T discovery_symbol [discovery_first]\n"
	    "1010 10 T discovery_tail [discovery_first]\n"
	    "2000 10 T discovery_symbol [discovery_second]\n"
	    "3000 10 T discovery_live [discovery_offline]\n";
	CHECK(dtrace_update(dtp) == 0);
	CHECK((second = dt_module_lookup_by_name(dtp, "discovery_second")) != NULL);
	CHECK(first->dm_btf_id == 101 && second->dm_btf_id == 202);
	CHECK(dt_module_symbol_by_name(dtp, second, "discovery_symbol") != NULL);
	CHECK(dt_module_symbol_by_name(dtp, offline, "discovery_live") != NULL);
	CHECK(!(offline->dm_flags & DT_DM_KERN_UNLOADED));
	CHECK(offline->dm_ctfp == types);
	count = found_first = found_second = 0;
	for (entry = dt_symbol_by_name(dtp, "discovery_symbol"); entry != NULL;
	     entry = dt_symbol_by_name_next(entry)) {
		count++;
		found_first += entry == sym;
		found_second += dt_symbol_module(entry) == second;
	}
	CHECK(count == 2 && found_first == 1 && found_second == 1);

	/* Disappearance and reappearance at a new address do not replace metadata. */
	for (i = 0; i < 4; i++) {
		symbols = i % 2 == 0 ? "" : fallback ?
		    "100 T _end\n9000 T discovery_symbol [discovery_first]\n" :
		    "100 1 T _end\n9000 80 T discovery_symbol [discovery_first]\n";
		CHECK(dtrace_update(dtp) == 0);
		CHECK(dtp->dt_exec == kernel);
		CHECK(kernel->dm_kernsyms == kernel_syms);
		CHECK(kernel->dm_text_addrs == kernel_ranges);
		CHECK(first->dm_kernsyms == first_syms);
		CHECK(first->dm_btf_id == 101 && second->dm_btf_id == 202);
		CHECK(first->dm_text_addrs == ranges);
		CHECK(first->dm_text_addrs_size == 1);
		CHECK(ranges->dar_va == 0x1000);
		CHECK(ranges->dar_size == (fallback ? 0x11 : 0x20));
		CHECK(dt_module_symbol_by_name(dtp, first, "discovery_symbol") == sym);
		dt_symbol_to_elfsym(dtp, sym, &elf);
		CHECK(elf.st_value == 0x1000);
		CHECK(dt_symbol_by_addr(first_syms, 0x9000) == NULL);
		CHECK(dt_module_symbol_by_name(dtp, second, "discovery_symbol") != NULL);
		CHECK(offline->dm_ctfp == types);
	}

	symbols = NULL;
	dtrace_close(dtp);
	return 0;
}

int
main(void)
{
	CHECK(check_discovery(0) == 0);
	CHECK(check_discovery(1) == 0);
	return 0;
}
