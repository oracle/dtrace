/*
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dt_module.h>

#define CHECK(expr) do { \
	if (!(expr)) { \
		fprintf(stderr, "Check failed at line %d: %s\n", __LINE__, #expr); \
		exit(1); \
	} \
} while (0)

/* No section names: the loader must use section types and sh_link. */
static void
write_elf(int fd, int bits, int tables)
{
	Elf *elf;
	GElf_Ehdr eh;
	char strings[] = "\0common\0private";
	char dynstrings[] = "\0dynamic\0common";
	union {
		Elf64_Sym s64[2][3];
		Elf32_Sym s32[2][3];
	} storage = { 0 };

	CHECK(ftruncate(fd, 0) == 0);
	CHECK((elf = elf_begin(fd, ELF_C_WRITE, NULL)) != NULL);
	CHECK(gelf_newehdr(elf, bits) != 0);
	CHECK(gelf_getehdr(elf, &eh) != NULL);
	eh.e_type = ET_DYN;
	eh.e_machine = bits == ELFCLASS64 ? EM_X86_64 : EM_386;
	CHECK(gelf_update_ehdr(elf, &eh) != 0);

	for (int t = 0; t < 2; t++) {
		Elf_Scn *strscn, *symscn;
		Elf_Data *strdata, *symdata;
		GElf_Shdr sh = { 0 };
		GElf_Sym sym = { 0 };
		size_t entsize = bits == ELFCLASS64 ? sizeof(Elf64_Sym) : sizeof(Elf32_Sym);

		if (!(tables & (1 << t)))
			continue;
		CHECK((strscn = elf_newscn(elf)) != NULL);
		CHECK((strdata = elf_newdata(strscn)) != NULL);
		strdata->d_buf = t == 0 ? strings : dynstrings;
		strdata->d_size = t == 0 ? sizeof(strings) : sizeof(dynstrings);
		strdata->d_type = ELF_T_BYTE;
		strdata->d_version = EV_CURRENT;
		sh.sh_type = SHT_STRTAB;
		sh.sh_addralign = 1;
		CHECK(gelf_update_shdr(strscn, &sh) != 0);

		CHECK((symscn = elf_newscn(elf)) != NULL);
		CHECK((symdata = elf_newdata(symscn)) != NULL);
		symdata->d_buf = bits == ELFCLASS64 ?
		    (void *)storage.s64[t] : (void *)storage.s32[t];
		symdata->d_size = 3 * entsize;
		symdata->d_type = ELF_T_SYM;
		symdata->d_version = EV_CURRENT;
		sh.sh_type = t == 0 ? SHT_SYMTAB : SHT_DYNSYM;
		sh.sh_link = elf_ndxscn(strscn);
		sh.sh_info = 1;
		sh.sh_entsize = entsize;
		sh.sh_addralign = bits == ELFCLASS64 ? 8 : 4;
		CHECK(gelf_update_shdr(symscn, &sh) != 0);
		CHECK(gelf_update_sym(symdata, 0, &sym) != 0);
		sym.st_name = t == 0 ? 1 : 9; /* common */
		sym.st_info = GELF_ST_INFO(STB_GLOBAL, STT_FUNC);
		sym.st_shndx = SHN_ABS;
		sym.st_value = t == 0 ? 0x1000 : 0x1100;
		sym.st_size = 16;
		CHECK(gelf_update_sym(symdata, 1, &sym) != 0);
		sym.st_name = t == 0 ? 8 : 1;
		sym.st_value = t == 0 ? 0x1200 : 0x1300;
		CHECK(gelf_update_sym(symdata, 2, &sym) != 0);
	}
	CHECK(elf_update(elf, ELF_C_WRITE) != -1);
	CHECK(elf_end(elf) == 0);
}

static int
count_symbol(void *arg, const GElf_Sym *sym, const char *name)
{
	int *counts = arg;

	if (strcmp(name, "common") == 0)
		counts[0]++;
	else if (strcmp(name, "private") == 0)
		counts[1]++;
	else if (strcmp(name, "dynamic") == 0)
		counts[2]++;
	else
		CHECK(0);
	return 0;
}

int
main(void)
{
	dtrace_hdl_t *dtp;
	int fd, err;
	char path[] = "/tmp/dtrace-elf-symbols-XXXXXX";

	CHECK(elf_version(EV_CURRENT) != EV_NONE);
	CHECK((fd = mkstemp(path)) >= 0);
	CHECK((dtp = dtrace_open(DTRACE_VERSION, 0, &err)) != NULL);

	for (int bits = ELFCLASS32; bits <= ELFCLASS64; bits++) {
		for (int tables = 0; tables <= 3; tables++) {
			for (int kernel = 0; kernel <= 1; kernel++) {
				dt_module_t *dmp;
				GElf_Sym sym;
				dtrace_syminfo_t si;
				char name[64];
				int counts[3] = { 0 };

				write_elf(fd, bits, tables);
				snprintf(name, sizeof(name), "elf_%d_%d_%d", bits, tables, kernel);
				CHECK((dmp = dt_module_create(dtp, name)) != NULL);
				strcpy(dmp->dm_file, path);
				if (kernel)
					dmp->dm_flags |= DT_DM_KERNEL | DT_DM_KERN_UNLOADED;

				if (tables == 0) {
					CHECK(dtrace_lookup_by_name(dtp, name, "common", &sym, &si) == -1);
					continue;
				}
				CHECK(dtrace_lookup_by_name(dtp, name, "common", &sym, &si) == 0);
				CHECK(sym.st_value == (tables & 1 ? 0x1000 : 0x1100));
				CHECK(strcmp(si.name, "common") == 0 && si.id == 1);

				if (tables & 1) {
					CHECK(dtrace_lookup_by_name(dtp, name, "private", &sym, &si) == 0);
					CHECK(sym.st_value == 0x1200 && si.id == 2);
				}
				if (tables & 2) {
					CHECK(dtrace_lookup_by_name(dtp, name, "dynamic", &sym, &si) == 0);
					CHECK(sym.st_value == 0x1300);
					CHECK(strcmp(si.name, "dynamic") == 0);
					CHECK(si.id == (tables & 1 ? 5 : 2));
				}
				CHECK(dt_module_sym_iter(dmp, count_symbol, counts) == 0);
				CHECK(counts[0] == 1);
				CHECK(counts[1] == !!(tables & 1));
				CHECK(counts[2] == !!(tables & 2));

				/* Give only this fixture a range for public address lookup. */
				CHECK((dmp->dm_text_addrs = calloc(1,
				    sizeof(dtrace_addr_range_t))) != NULL);
				dmp->dm_text_addrs[0].dar_va = 0x1000;
				dmp->dm_text_addrs[0].dar_size = 0x400;
				dmp->dm_text_addrs_size = 1;
				dtp->dt_modranges_valid = 0;

				for (int t = 0; t < 2; t++) {
					GElf_Addr addr = t == 0 ? 0x1204 : 0x1304;
					int found = dtrace_lookup_by_addr(dtp, addr, &sym, &si);

					CHECK((found == 0) == !!(tables & (1 << t)));
					if (found == 0) {
						CHECK(sym.st_value == addr - 4);
						CHECK(strcmp(si.name, t == 0 ? "private" : "dynamic") == 0);
						CHECK(si.id == (t == 1 && (tables & 1) ? 5 : 2));
					}
				}
				CHECK(dtrace_lookup_by_addr(dtp, 0x13ff, &sym, &si) == -1);
				dmp->dm_text_addrs_size = 0;
				dtp->dt_modranges_valid = 0;
			}
		}
	}
	dtrace_close(dtp);

	/* An unloaded module still needs ELF symbols when its CTF is cached. */
	CHECK((dtp = dtrace_open(DTRACE_VERSION, 0, &err)) != NULL);
	{
		dt_module_t *shared = dt_module_create(dtp, "shared_ctf");
		dt_module_t *dmp = dt_module_create(dtp, "elf_cached_ctf");
		GElf_Sym sym;

		CHECK(shared != NULL && dmp != NULL);
		CHECK((shared->dm_ctfp = ctf_create(&err)) != NULL);
		CHECK((dmp->dm_ctfp = ctf_create(&err)) != NULL);
		dmp->dm_flags |= DT_DM_KERNEL | DT_DM_KERN_UNLOADED;
		dmp->dm_ops = NULL;
		strcpy(dmp->dm_file, path);
		write_elf(fd, ELFCLASS64, 3);
		CHECK(dtrace_lookup_by_name(dtp, dmp->dm_name, "common",
		    &sym, NULL) == 0);
		CHECK(sym.st_value == 0x1000);
	}
	dtrace_close(dtp);
	close(fd);
	unlink(path);
	return 0;
}
