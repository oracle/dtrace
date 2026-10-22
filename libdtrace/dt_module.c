/*
 * Oracle Linux DTrace.
 * Copyright (c) 2009, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <sys/types.h>
#include <elf.h>

#include <fcntl.h>
#include <sys/stat.h>

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <libelf.h>
#include <limits.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <port.h>

#include <zlib.h>
#include <tracefs.h>

#include <dt_kernel_module.h>
#include <dt_module.h>
#include <dt_impl.h>
#include <dt_string.h>
#include <dt_btf.h>

#define KSYM_NAME_MAX 128		    /* from kernel/scripts/kallsyms.c */
#define GZCHUNKSIZE (1024*512)		    /* gzip uncompression chunk size */

static void
dt_module_destroy(dtrace_hdl_t *dtp, dt_module_t *dmp);

static void
dt_module_clear_elf(dt_module_t *dmp);

static void
dt_module_shuffle_to_start(dtrace_hdl_t *dtp, const char *name);

static void
dt_kern_module_find_btf(dtrace_hdl_t *dtp, dt_module_t *dmp);

static void
dt_kern_module_find_ctf(dtrace_hdl_t *dtp, dt_module_t *dmp);

typedef struct dt_path {
	dt_list_t		list;
	struct dt_hentry	he;
	const char		*str;
} dt_path_t;

static uint32_t
dt_path_hval(const dt_path_t *path)
{
	return str2hval(path->str, 0);
}

static int
dt_path_cmp(const dt_path_t *p, const dt_path_t *q)
{
	return strcmp(p->str, q->str);
}

DEFINE_HE_STD_LINK_FUNCS(dt_path, dt_path_t, he)
DEFINE_HTAB_STD_OPS(dt_path)

static int
dt_user_path_add(dtrace_hdl_t *dtp, dt_htab_t *h, char *s)
{
	dt_path_t	tmpl, *path;

	if (s == NULL)
		return -1;

	tmpl.str = s;
	if (dt_htab_lookup(h, &tmpl) != NULL) {
		free(s);
		return 0;
	}

	path = malloc(sizeof(dt_path_t));
	if (path == NULL)
		goto fail;

	memset(path, 0, sizeof(dt_path_t));
	path->str = s;
	if (dt_htab_insert(h, path) != 0) {
		free(path);
		goto fail;
	}

	dt_dprintf("Adding user module path %s\n", path->str);
	dt_list_append(&dtp->dt_user_path, path);

	return 0;

fail:
	free(s);
	return -1;
}

static int
dt_user_path_add_env(dtrace_hdl_t *dtp, dt_htab_t *h, const char *env)
{
	char	*str, *end;

	str = getenv(env);
	if (str == NULL)
		return 0;

	for (;;) {
		end = strchr(str, ':');
		if (end == NULL) {
			/*
			 * A trailing empty path component denotes the current
			 * directory.
			 */
			if (*str == '\0')
				return dt_user_path_add(dtp, h, strdup("."));

			return dt_user_path_add(dtp, h, strdup(str));
		}

		if (end == str) {
			if (dt_user_path_add(dtp, h, strdup(".")) == -1)
				return -1;
		} else
			if (dt_user_path_add(dtp, h, strndup(str, end - str)) == -1)
				return -1;

		str = end + 1;
	}

	return 0;
}

static int
dt_user_path_init(dtrace_hdl_t *dtp)
{
	dt_htab_t	*h = dt_htab_create(&dt_path_htab_ops);
	FILE		*fp;
	char		*buf = NULL;
	size_t		len = 0;
	char		*str, *end;
	int		rc = 0;

	if (h == NULL)
		goto fail;

	/* Add paths from $LD_LIBRARY_PATH. */
	if (dt_user_path_add_env(dtp, h, "LD_LIBRARY_PATH") != 0)
		goto fail;

	/* Add paths from ld.so.cache (using ldconfig -p output). */
	fp = popen("/sbin/ldconfig -p", "r");
	if (fp == NULL) {
		dt_dprintf("Failed to get paths from ldconfig\n");
		goto path;
	}

	while (getline(&buf, &len, fp) != -1) {
		/* We are interested in lines like: <something> => /path */
		str = strstr(buf, "=>");
		if (str == NULL)
			continue;

		str += 2;
		while (isspace(*str))
			str++;

		end = str + strcspn(str, "\n");
		while (end > str && isspace(end[-1]))
			end--;
		*end = '\0';

		end = strrchr(str, '/');
		if (end == NULL)
			continue;

		if (dt_user_path_add(dtp, h, strndup(str, end == str ? 1 : end - str)) == -1) {
			rc = -1;
			break;
		}
	}

	free(buf);
	pclose(fp);

	if (rc == -1)
		goto fail;

path:
	/* Add paths from $PATH. */
	if (dt_user_path_add_env(dtp, h, "PATH") != 0)
		goto fail;

	/* Always add . in case it wasn't added yet. */
	if (dt_user_path_add(dtp, h, strdup(".")) == -1)
		goto fail;

	dt_htab_destroy(h);
	return 0;

fail:
	dt_htab_destroy(h);
	dt_user_path_destroy(dtp);
	dt_set_errno(dtp, ENOMEM);
	return -1;
}

void
dt_user_path_destroy(dtrace_hdl_t *dtp)
{
	dt_path_t	*path;

	while ((path = dt_list_next(&dtp->dt_user_path)) != NULL) {
		dt_list_delete(&dtp->dt_user_path, path);
		free((char *)path->str);
		free(path);
	}
}

static uint32_t
dt_module_hval(const dt_module_t *mod)
{
	return str2hval(mod->dm_name, 0);
}

static int
dt_module_cmp(const dt_module_t *p, const dt_module_t *q)
{
	return strcmp(p->dm_name, q->dm_name);
}

DEFINE_HE_STD_LINK_FUNCS(dt_module, dt_module_t, dm_he)

static void
dt_module_ranges_invalidate(dtrace_hdl_t *dtp)
{
	free(dtp->dt_modranges);
	dtp->dt_modranges = NULL;
	dtp->dt_modranges_count = 0;
	dtp->dt_modranges_valid = 0;
}

static void *
dt_module_del_mod(dt_module_t *head, dt_module_t *dmp)
{
	dt_module_ranges_invalidate(dmp->dm_dtp);
	head = dt_module_del(head, dmp);
	dt_list_delete(&dmp->dm_dtp->dt_modlist, dmp);

	dt_module_destroy(dmp->dm_dtp, dmp);
	free(dmp);

	return head;
}

static dt_htab_ops_t dt_module_htab_ops = {
	.hval = (htab_hval_fn)dt_module_hval,
	.cmp = (htab_cmp_fn)dt_module_cmp,
	.add = (htab_add_fn)dt_module_add,
	.del = (htab_del_fn)dt_module_del_mod,
	.next = (htab_next_fn)dt_module_next
};

/*
 * Symbol table management for ELF-backed modules.
 */

static void
dt_module_symhash_insert(dt_module_symbols_t *dmp, const char *name, uint_t id)
{
	dt_modsym_t *dsp = &dmp->dm_symchains[dmp->dm_symfree];
	uint_t h;

	assert(dmp->dm_symfree < dmp->dm_nsymelems + 1);

	dsp->dms_symid = id;
	h = str2hval(name, 0) % dmp->dm_nsymbuckets;
	dsp->dms_next = dmp->dm_symbuckets[h];
	dmp->dm_symbuckets[h] = dmp->dm_symfree++;
}

static GElf_Sym *
dt_module_symgelf32(const Elf32_Sym *src, GElf_Sym *dst)
{
	if (dst != NULL) {
		dst->st_name = src->st_name;
		dst->st_info = src->st_info;
		dst->st_other = src->st_other;
		dst->st_shndx = src->st_shndx;
		dst->st_value = src->st_value;
		dst->st_size = src->st_size;
	}

	return dst;
}

static GElf_Sym *
dt_module_symgelf64(const Elf64_Sym *src, GElf_Sym *dst)
{
	if (dst != NULL)
		memcpy(dst, src, sizeof(GElf_Sym));

	return dst;
}

#ifdef BITS
#error BITS is already defined: look to your header files!
#endif

#define BITS 32
#include <dt_symbol_modops.h>
#undef BITS
#define BITS 64
#include <dt_symbol_modops.h>
#undef BITS

/*
 * Keep the ELF tables independent so .symtab IDs remain valid CTF IDs.
 * Dynamic symbols occupy the public ID range following .symtab.
 */
static GElf_Sym *
dt_module_symname(dt_module_t *dmp, const char *name, GElf_Sym *symp,
    uint_t *idp)
{
	uint_t id;

	if (dmp->dm_ops->do_symname(&dmp->dm_symbols[0], name, symp, &id)) {
		if (idp != NULL)
			*idp = id;
		return symp;
	}
	if (dmp->dm_ops->do_symname(&dmp->dm_symbols[1], name, symp, &id)) {
		if (idp != NULL)
			*idp = dmp->dm_symbols[0].dm_nsymelems + id;
		return symp;
	}
	return NULL;
}

static GElf_Sym *
dt_module_symaddr(dt_module_t *dmp, GElf_Addr addr, GElf_Sym *symp,
    uint_t *idp)
{
	uint_t id;

	if (dmp->dm_ops->do_symaddr(&dmp->dm_symbols[0], addr, symp, &id)) {
		if (idp != NULL)
			*idp = id;
		return symp;
	}
	if (dmp->dm_ops->do_symaddr(&dmp->dm_symbols[1], addr, symp, &id)) {
		if (idp != NULL)
			*idp = dmp->dm_symbols[0].dm_nsymelems + id;
		return symp;
	}
	return NULL;
}

static const char *
dt_module_symstr(dt_module_t *dmp, uint_t id, const GElf_Sym *sym)
{
	int t = id >= dmp->dm_symbols[0].dm_nsymelems;

	return (const char *)dmp->dm_symbols[t].dm_strtab.cts_data + sym->st_name;
}

dt_module_t *
dt_module_create(dtrace_hdl_t *dtp, const char *name)
{
	dt_module_t *dmp;

	if (!dtp->dt_mods) {
		dtp->dt_mods = dt_htab_create(&dt_module_htab_ops);
		if (!dtp->dt_mods)
			return NULL;
	}

	if ((dmp = dt_module_lookup_by_name(dtp, name)) != NULL)
		return dmp;

	if ((dmp = malloc(sizeof(dt_module_t))) == NULL)
		return NULL; /* caller must handle allocation failure */

	memset(dmp, 0, sizeof(dt_module_t));
	strlcpy(dmp->dm_name, name, sizeof(dmp->dm_name));
	if (dt_htab_insert(dtp->dt_mods, dmp) < 0) {
		free(dmp);
		return NULL;
	}
	dt_list_append(&dtp->dt_modlist, dmp);

	if (dtp->dt_conf.dtc_ctfmodel == CTF_MODEL_LP64)
		dmp->dm_ops = &dt_modops_64;
	else
		dmp->dm_ops = &dt_modops_32;
	dmp->dm_dtp = dtp;
	if (strcmp(name, "shared_ctf") == 0) {
		dmp->dm_flags |= DT_DM_KERNEL | DT_DM_SHARED;
		dmp->dm_ops = NULL;
	}

	dt_dprintf("created module %s\n", dmp->dm_name);

	return dmp;
}

/*
 * Create a userspace module for an ELF object.  Userspace module names are
 * resolved based on the LD_LIBRARY_PATH, ld.so.cache entries, and PATH.
 * Module loading is lazy, as it is for all other modules.
 */
dt_module_t *
dt_module_create_user(dtrace_hdl_t *dtp, const char *name)
{
	dt_module_t	*dmp;
	char		*file = NULL;
	char		*p;

	if (name == NULL || name[0] == '\0') {
		dt_set_errno(dtp, EINVAL);
		return NULL;
	}

	if (dt_list_empty(&dtp->dt_user_path) && dt_user_path_init(dtp) == -1)
		return NULL;

	if (!dt_list_empty(&dtp->dt_user_path)) {
		dt_path_t	*path;

		for (path = dt_list_next(&dtp->dt_user_path); path != NULL;
		     path = dt_list_next(path)) {
			if (asprintf(&file, "%s/%s", path->str, name) == -1) {
				dt_set_errno(dtp, ENOMEM);
				return NULL;
			}
			if (access(file, R_OK) == 0)
				break;

			free(file);
			file = NULL;
		}

		if (file == NULL) {
			dt_set_errno(dtp, EDT_NOMOD);
			return NULL;
		}
	}

	/*
	 * Use the resolved pathname as the object location, but name it based
	 * on just the filename.
	 */
	p = strrchr(name, '/');
	if (p != NULL)
		name = p + 1;

	if (strcmp(name, "shared_ctf") == 0) {
		dt_set_errno(dtp, EEXIST);
		goto fail;
	}

	if (strlen(name) >= DTRACE_MODNAMELEN ||
	    strlen(file) >= sizeof(dmp->dm_file)) {
		dt_set_errno(dtp, ENAMETOOLONG);
		goto fail;
	}

	dmp = dt_module_lookup_by_name(dtp, name);
	if (dmp != NULL) {
		/* Do not turn a kernel or built-in type module into a user module. */
		if (dmp->dm_flags & DT_DM_KERNEL || dmp == dtp->dt_cdefs ||
		    dmp == dtp->dt_ddefs ||
		    (dmp->dm_file[0] != '\0' && strcmp(dmp->dm_file, file) != 0)) {
			dt_set_errno(dtp, EEXIST);
			goto fail;
		}
	} else if ((dmp = dt_module_create(dtp, name)) == NULL) {
		dt_set_errno(dtp, EDT_NOMEM);
		goto fail;
	}

	strlcpy(dmp->dm_file, file, sizeof(dmp->dm_file));
	free(file);
	return dmp;

fail:
	free(file);
	return NULL;
}

dt_module_t *
dt_module_lookup_by_name(dtrace_hdl_t *dtp, const char *name)
{
	dt_module_t tmpl;

	if (strlen(name) > (DTRACE_MODNAMELEN - 1))
		return NULL;

	if (!dtp->dt_mods)
		return NULL;			/* no modules yet */

	/* 'genunix' is an alias for 'vmlinux'. */

	if (strcmp(name, "genunix") == 0) {
		name = "vmlinux";
	}

	strcpy(tmpl.dm_name, name);
	return dt_htab_lookup(dtp->dt_mods, &tmpl);
}

/*ARGSUSED*/
dt_module_t *
dt_module_lookup_by_ctf(dtrace_hdl_t *dtp, ctf_file_t *ctfp)
{
	return ctfp ? ctf_getspecific(ctfp) : NULL;
}

/* Return the shared dictionary without loading or creating a module. */
ctf_file_t *
dt_module_shared_ctf(dtrace_hdl_t *dtp)
{
	dt_module_t *dmp = dt_module_lookup_by_name(dtp, "shared_ctf");

	return dmp == NULL ? NULL : dmp->dm_ctfp;
}

static void
dt_module_delete_registered(dtrace_hdl_t *dtp, dt_module_t *dmp)
{
	if (dt_module_lookup_by_name(dtp, dmp->dm_name) == dmp)
		dt_htab_delete(dtp->dt_mods, dmp);
}

static int
dt_module_init_elf_impl(dtrace_hdl_t *dtp, dt_module_t *dmp,
    int unregister_on_error)
{
	int fd, err, bits;
	size_t shstrs;

	if (dmp->dm_file[0] == '\0') {
		dt_dprintf("failed to open ELF file for module %s: "
		    "no file name known\n", dmp->dm_name);
		return dt_set_errno(dtp, EDT_NOMOD);
	}

	if ((fd = open(dmp->dm_file, O_RDONLY)) == -1) {
		dt_dprintf("failed to open module %s at %s: %s\n",
		    dmp->dm_name, dmp->dm_file, strerror(errno));
		return dt_set_errno(dtp, EDT_OBJIO);
	}

	/*
	 * Don't hold the fd open forever. (ELF_C_READ followed by
	 * elf_cntl(..., ELF_C_FDREAD) triggers assertion failures in elfutils
	 * at gelf_getshdr() time: ELF_C_READ_MMAP works around this.)
	 */

	dmp->dm_elf = elf_begin(fd, ELF_C_READ_MMAP, NULL);
	err = elf_cntl(dmp->dm_elf, ELF_C_FDREAD);
	close(fd);

	if (dmp->dm_elf == NULL || err == -1 ||
	    elf_getshdrstrndx(dmp->dm_elf, &shstrs) == -1) {
		dt_dprintf("failed to load %s: %s\n", dmp->dm_file,
		    elf_errmsg(elf_errno()));
		if (unregister_on_error)
			dt_module_delete_registered(dtp, dmp);
		return dt_set_errno(dtp, EDT_OBJIO);
	}

	switch (gelf_getclass(dmp->dm_elf)) {
	case ELFCLASS32:
		dmp->dm_ops = &dt_modops_32;
		bits = 32;
		break;
	case ELFCLASS64:
		dmp->dm_ops = &dt_modops_64;
		bits = 64;
		break;
	default:
		dt_dprintf("failed to load %s: unknown ELF class\n",
		    dmp->dm_file);
		if (unregister_on_error)
			dt_module_delete_registered(dtp, dmp);
		return dt_set_errno(dtp, EDT_ELFCLASS);
	}

	dt_dprintf("opened %d-bit module %s (%s)\n", bits, dmp->dm_name,
	    dmp->dm_file);

	return 0;
}

static int
dt_module_init_elf(dtrace_hdl_t *dtp, dt_module_t *dmp)
{
	return dt_module_init_elf_impl(dtp, dmp, 1);
}

static int
dt_module_load_sect(dtrace_hdl_t *dtp, dt_module_t *dmp, ctf_sect_t *ctsp)
{
	const char *s;
	size_t shstrs;
	GElf_Shdr sh;
	Elf_Data *dp;
	Elf_Scn *sp;

	if (elf_getshdrstrndx(dmp->dm_elf, &shstrs) == -1)
		return dt_set_errno(dtp, EDT_NOMOD);

	for (sp = NULL; (sp = elf_nextscn(dmp->dm_elf, sp)) != NULL; ) {
		if (gelf_getshdr(sp, &sh) == NULL || sh.sh_type == SHT_NULL ||
		    (s = elf_strptr(dmp->dm_elf, shstrs, sh.sh_name)) == NULL)
			continue; /* skip any malformed sections */

#ifdef HAVE_LIBCTF
		if (sh.sh_entsize == ctsp->cts_entsize &&
		    strcmp(s, ctsp->cts_name) == 0)
			break; /* section matches specification */
#else
		if (sh.sh_type == ctsp->cts_type &&
		    sh.sh_entsize == ctsp->cts_entsize &&
		    strcmp(s, ctsp->cts_name) == 0)
			break; /* section matches specification */
#endif
	}

	/*
	 * If the section isn't found, return success but leave cts_data set
	 * to NULL and cts_size set to zero for our caller.
	 */
	if (sp == NULL || (dp = elf_getdata(sp, NULL)) == NULL)
		return 0;

	ctsp->cts_data = dp->d_buf;
	ctsp->cts_size = dp->d_size;

	dt_dprintf("loaded %s [%s] (%lu bytes)\n",
	    dmp->dm_name, ctsp->cts_name, (ulong_t)ctsp->cts_size);

	return 0;
}

#ifdef HAVE_LIBCTF
/*
 * libctf-nobfd cannot use the BFD-backed ctf_arc_open() helper, so extract
 * the .ctf section ourselves and open that with ctf_arc_bufopen().
 */
static ctf_archive_t *
dt_ctfa_open(dtrace_hdl_t *dtp, const char *ctfa_name)
{
	ctf_archive_t	*ctfa = NULL;
	ctf_sect_t	ctsp = { 0 };
	dt_module_t	*dmp;
	char		dm_file[PATH_MAX];
	const dt_modops_t	*dm_ops;
	Elf		*dm_elf;
	void		*ctfa_data = NULL;

	dmp = dtp->dt_exec;
	if (dmp == NULL || strcmp(dmp->dm_name, "vmlinux") != 0)
		dmp = dt_module_lookup_by_name(dtp, "vmlinux");
	if (dmp == NULL) {
		dtp->dt_ctferr = ECTF_INTERNAL;
		return NULL;
	}

	/*
	 * Borrow the registered vmlinux module as the ELF loader context, but
	 * leave its persistent module state exactly as we found it.
	 */
	memcpy(dm_file, dmp->dm_file, sizeof(dm_file));
	dm_ops = dmp->dm_ops;
	dm_elf = dmp->dm_elf;

	if (strlcpy(dmp->dm_file, ctfa_name, sizeof(dmp->dm_file)) >=
	    sizeof(dmp->dm_file)) {
		dtp->dt_ctferr = ENAMETOOLONG;
		goto out;
	}

	errno = 0;
	if (dt_module_init_elf_impl(dtp, dmp, 0) != 0) {
		dtp->dt_ctferr = errno != 0 ? errno : ECTF_FMT;
		goto out;
	}

	ctsp.cts_name = ".ctf";
	ctsp.cts_entsize = 0;

	if (dt_module_load_sect(dtp, dmp, &ctsp) == -1) {
		dtp->dt_ctferr = ECTF_FMT;
		goto out;
	}

	if (ctsp.cts_data == NULL || ctsp.cts_size == 0) {
		dtp->dt_ctferr = ECTF_NOCTFDATA;
		goto out;
	}

	ctfa_data = malloc(ctsp.cts_size);
	if (ctfa_data == NULL) {
		dtp->dt_ctferr = errno;
		goto out;
	}
	memcpy(ctfa_data, ctsp.cts_data, ctsp.cts_size);
	ctsp.cts_data = ctfa_data;

	ctfa = ctf_arc_bufopen(&ctsp, NULL, NULL, &dtp->dt_ctferr);
	if (ctfa != NULL) {
		free(dtp->dt_ctfa_data);
		dtp->dt_ctfa_data = ctfa_data;
		ctfa_data = NULL;
	}

out:
	free(ctfa_data);
	if (dmp->dm_elf != dm_elf) {
		if (dmp->dm_elf != NULL)
			elf_end(dmp->dm_elf);
		dmp->dm_elf = dm_elf;
	}
	dmp->dm_ops = dm_ops;
	memcpy(dmp->dm_file, dm_file, sizeof(dmp->dm_file));
	return ctfa;
}
#endif

/*
 * Only used for linked-in modules.  Archived modules are uncompressed
 * automatically.
 */
static void *dt_ctf_uncompress(dt_module_t *dmp, ctf_sect_t *ctsp)
{
	z_stream s;
	int ret;
	unsigned char out[GZCHUNKSIZE];
	char *output = NULL;
	size_t out_size = 0;

	s.opaque = Z_NULL;
	s.zalloc = Z_NULL;
	s.zfree = Z_NULL;
	s.avail_in = ctsp->cts_size;
	s.next_in = (void *)ctsp->cts_data;
	s.next_out = out;
	s.avail_out = GZCHUNKSIZE;

	switch (inflateInit2(&s, 15 + 32)) {
	case Z_OK: break;
	case Z_MEM_ERROR: goto oom;
	default: goto zerr;
	}

	do {
		char *new_output;
		ret = inflate(&s, Z_NO_FLUSH);
		switch (ret) {
		case Z_STREAM_END:
			break;
		case Z_BUF_ERROR:
		case Z_OK:
			if (s.avail_out == GZCHUNKSIZE) {
				s.msg = "no output possible after inflate round";
				goto zerr;
			}
			break;
		case Z_DATA_ERROR:
			inflateEnd(&s);
			goto uncompressed;
		case Z_MEM_ERROR:
			goto oom;
		default:
			goto zerr;
		}

		new_output = realloc(output, out_size +
		    (GZCHUNKSIZE - s.avail_out));

		if (new_output == NULL)
			goto oom;
		output = new_output;

		memcpy(output + out_size, out, (GZCHUNKSIZE - s.avail_out));
		out_size += (GZCHUNKSIZE - s.avail_out);
		s.next_out = out;
		s.avail_out = GZCHUNKSIZE;

	} while ((ret != Z_STREAM_END) && (ret != Z_BUF_ERROR));

	inflateEnd(&s);
	ctsp->cts_size = out_size;
	ctsp->cts_data = output;

	return output;

 uncompressed:
	free(output);
	return NULL;

 zerr:
	inflateEnd(&s);
	free(output);
	dt_dprintf("CTF decompression error in module %s: %s\n", dmp->dm_name,
	    s.msg);
	ctsp->cts_data = NULL;
	ctsp->cts_size = 0;
	return NULL;

 oom:
	free(output);
	dt_dprintf("Out of memory decompressing CTF section for module %s.\n",
	    dmp->dm_name);
	ctsp->cts_data = NULL;
	ctsp->cts_size = 0;
	return NULL;
}

/* Load a symbol table and its linked string table, including stripped ELFs. */
static int
dt_module_load_symbols(dtrace_hdl_t *dtp, dt_module_t *dmp,
    dt_module_symbols_t *syms, uint_t type)
{
	Elf_Scn *scn = NULL, *strscn;
	Elf_Data *data, *strdata;
	GElf_Shdr sh, strsh;
	size_t entsize = dmp->dm_ops == &dt_modops_64 ?
	    sizeof(Elf64_Sym) : sizeof(Elf32_Sym);

	while ((scn = elf_nextscn(dmp->dm_elf, scn)) != NULL) {
		if (gelf_getshdr(scn, &sh) == NULL || sh.sh_type != type)
			continue;

		if (sh.sh_entsize != entsize || sh.sh_size % entsize != 0 ||
		    sh.sh_size / entsize >= UINT_MAX ||
		    (strscn = elf_getscn(dmp->dm_elf, sh.sh_link)) == NULL ||
		    gelf_getshdr(strscn, &strsh) == NULL ||
		    strsh.sh_type != SHT_STRTAB ||
		    (data = elf_getdata(scn, NULL)) == NULL ||
		    (strdata = elf_getdata(strscn, NULL)) == NULL ||
		    data->d_size != sh.sh_size || strdata->d_size != strsh.sh_size ||
		    (data->d_size != 0 && data->d_buf == NULL) ||
		    (strdata->d_size != 0 && strdata->d_buf == NULL))
			return dt_set_errno(dtp, EDT_NOMOD);

		syms->dm_symtab.cts_name = type == SHT_SYMTAB ? ".symtab" : ".dynsym";
		syms->dm_symtab.cts_data = data->d_buf;
		syms->dm_symtab.cts_size = data->d_size;
		syms->dm_symtab.cts_entsize = entsize;
		syms->dm_strtab.cts_name = type == SHT_SYMTAB ? ".strtab" : ".dynstr";
		syms->dm_strtab.cts_data = strdata->d_buf;
		syms->dm_strtab.cts_size = strdata->d_size;
#ifndef HAVE_LIBCTF
		syms->dm_symtab.cts_type = type;
		syms->dm_strtab.cts_type = SHT_STRTAB;
#endif
		syms->dm_nsymelems = data->d_size / entsize;
		if (syms->dm_nsymelems == 0)
			return 0;

		syms->dm_nsymbuckets = _dtrace_strbuckets;
		syms->dm_symfree = 1;
		syms->dm_symbuckets = calloc(syms->dm_nsymbuckets, sizeof(uint_t));
		syms->dm_symchains = calloc((size_t)syms->dm_nsymelems + 1,
		    sizeof(dt_modsym_t));
		if (syms->dm_symbuckets == NULL || syms->dm_symchains == NULL)
			return dt_set_errno(dtp, EDT_NOMEM);

		syms->dm_asrsv = dmp->dm_ops->do_syminit(syms);
		if (syms->dm_asrsv != 0) {
			syms->dm_asmap = calloc(syms->dm_asrsv, sizeof(void *));
			if (syms->dm_asmap == NULL)
				return dt_set_errno(dtp, EDT_NOMEM);
			dmp->dm_ops->do_symsort(syms);
		}

		dt_dprintf("loaded %s [%s] (%u symbols, %u addresses)\n",
		    dmp->dm_name, syms->dm_symtab.cts_name,
		    syms->dm_symfree - 1, syms->dm_aslen);
		return 0;
	}

	return 0;
}

static int
dt_module_load(dtrace_hdl_t *dtp, dt_module_t *dmp)
{
	if (dmp->dm_flags & DT_DM_LOADED)
		return 0; /* module is already loaded */

	/*
	 * A non-kernel module with a known file was explicitly created as a
	 * userspace module.  Its CTF, if any, is in that ELF object; do not let
	 * kernel module discovery reclassify it as an unloaded kernel module.
	 */
	if ((dmp->dm_flags & DT_DM_KERNEL) || dmp->dm_file[0] == '\0') {
		/* Load BTF data for the kernel module. */
		if (!(dmp->dm_flags & DT_DM_SHARED))
			dt_kern_module_find_btf(dtp, dmp);

		/*
		 * Find where the kernel module is and preliminarily load its CTF.
		 * If this fails, dt_module_init_elf() reports the error below.
		 */
		dt_kern_module_find_ctf(dtp, dmp);
	}

	/* An archive- or BTF-backed shared dictionary has no ELF symbols. */
	if (dmp->dm_flags & DT_DM_SHARED) {
		if (dmp->dm_ctfp != NULL) {
			dmp->dm_flags |= DT_DM_LOADED;
			return 0;
		}
		if (dmp->dm_file[0] == '\0')
			return dt_set_errno(dtp, EDT_NOCTF);
	}

	/*
	 * Modules not found in the CTF archive, including non-kernel modules,
	 * must pull in the symbol and string tables (in addition to, perhaps,
	 * the CTF section.)
	 */

	if (dmp->dm_ctfp == NULL) {
		if ((dmp->dm_elf == NULL) && (dt_module_init_elf(dtp, dmp) != 0))
			return -1; /* dt_errno is set for us */

		dmp->dm_ctdata.cts_name = ".ctf";

#ifndef HAVE_LIBCTF
		dmp->dm_ctdata.cts_type = SHT_PROGBITS;
		dmp->dm_ctdata.cts_flags = 0;
		dmp->dm_ctdata.cts_offset = 0;
#endif
		dmp->dm_ctdata.cts_data = NULL;
		dmp->dm_ctdata.cts_size = 0;
		dmp->dm_ctdata.cts_entsize = 0;

		/*
		 * Attempt to load and uncompress the module's CTF section.
		 * Note that modules might not contain CTF data: this will
		 * result in a successful load_sect but data of size zero (or,
		 * alas, 1, thanks to a workaround for a bug in objcopy in
		 * binutils 2.20).  We will then fail if dt_module_getctf() is
		 * called, as shown below.
		 */

		if (dt_module_load_sect(dtp, dmp, &dmp->dm_ctdata) == -1) {
			dt_module_clear_elf(dmp);
			return -1; /* dt_errno is set for us */
		}

		/*
		 * The CTF section is often gzip-compressed.  Uncompress it.
		 */
		if (dmp->dm_ctdata.cts_size > 1)
			dmp->dm_ctdata_data = dt_ctf_uncompress(dmp,
			    &dmp->dm_ctdata);
	}

	/*
	 * Nothing more to do for loaded kernel modules: we already have their
	 * symbols loaded into the dm_kernsyms.
	 */

	if ((dmp->dm_flags & DT_DM_KERNEL) &&
	    (!(dmp->dm_flags & DT_DM_KERN_UNLOADED))) {
		dmp->dm_flags |= DT_DM_LOADED;
		return 0;
	}

	/* Cached CTF does not supply the ELF symbols of an unloaded module. */
	if (dmp->dm_elf == NULL && dt_module_init_elf(dtp, dmp) != 0)
		return -1; /* dt_errno is set for us */

	if (dt_module_load_symbols(dtp, dmp, &dmp->dm_symbols[0],
	    SHT_SYMTAB) == -1 ||
	    dt_module_load_symbols(dtp, dmp, &dmp->dm_symbols[1],
	    SHT_DYNSYM) == -1) {
		dt_module_clear_elf(dmp);
		return -1;
	}

	/* Keep the combined public symbol ID space representable. */
	if (dmp->dm_symbols[1].dm_nsymelems >
	    UINT_MAX - dmp->dm_symbols[0].dm_nsymelems)
		goto oom;

	dmp->dm_flags |= DT_DM_LOADED;
	return 0;
oom:
	dt_module_clear_elf(dmp);
	return dt_set_errno(dtp, EDT_NOMEM);
}

/*
 * Get the CTF of a kernel module with CTF linked in.
 */
ctf_file_t *
dt_module_getctf(dtrace_hdl_t *dtp, dt_module_t *dmp)
{
	const char *parent;
	int model;
	dt_module_symbols_t *syms;

	if (!(dmp->dm_flags & DT_DM_LOADED))
		if (dt_module_load(dtp, dmp) != 0)
			return NULL;

	if (dmp->dm_ctfp != NULL)
		return dmp->dm_ctfp;

	assert(!(dmp->dm_flags & DT_DM_CTF_ARCHIVED));

	if ((dmp->dm_ops == &dt_modops_64) || (dmp->dm_ops == NULL))
		model = CTF_MODEL_LP64;
	else
		model = CTF_MODEL_ILP32;

	/*
	 * If the data model of the module does not match our program data
	 * model, then do not permit CTF from this module to be opened and
	 * returned to the compiler.  If we support mixed data models in the
	 * future for combined kernel/user tracing, this can be removed.
	 */
	if (dtp->dt_conf.dtc_ctfmodel != model) {
		dt_set_errno(dtp, EDT_DATAMODEL);
		return NULL;
	}

	if ((dmp->dm_ctdata.cts_size == 0) ||
	    (dmp->dm_ctdata.cts_size == 1)) {
		dt_set_errno(dtp, EDT_NOCTF);
		return NULL;
	}

	syms = &dmp->dm_symbols[dmp->dm_symbols[0].dm_nsymelems == 0];
	if (dmp->dm_flags & DT_DM_KERNEL)
		dmp->dm_ctfp = ctf_bufopen(&dmp->dm_ctdata, NULL, NULL,
		    &dtp->dt_ctferr);
	else
		dmp->dm_ctfp = ctf_bufopen(&dmp->dm_ctdata,
		    &syms->dm_symtab, &syms->dm_strtab,
		    &dtp->dt_ctferr);

	if (dmp->dm_ctfp == NULL) {
		dt_dprintf("ctf loading for module %s failed: error: %s\n",
		    dmp->dm_name, ctf_errmsg(dtp->dt_ctferr));
		dt_set_errno(dtp, EDT_CTF);
		return NULL;
	}

	ctf_setmodel(dmp->dm_ctfp, model);

	if ((parent = ctf_parent_name(dmp->dm_ctfp)) != NULL) {
		assert(strcmp(parent, "shared_ctf") == 0);
		if (ctf_import(dmp->dm_ctfp, dt_module_shared_ctf(dtp)) < 0) {
			dt_dprintf("Importing of CTF for %s failed: %s\n",
			    dmp->dm_name, ctf_errmsg(ctf_errno(dmp->dm_ctfp)));
			dtp->dt_ctferr = ctf_errno(dmp->dm_ctfp);
			ctf_close(dmp->dm_ctfp);
			dmp->dm_ctfp = NULL;
			return NULL;
		}
	}

	ctf_setspecific(dmp->dm_ctfp, dmp);

	dt_dprintf("loaded CTF container for %s (%p)\n",
	    dmp->dm_name, (void *)dmp->dm_ctfp);

	return dmp->dm_ctfp;
}

/*
 * Discard a partial ELF load without invalidating persistent kernel metadata.
 */
static void
dt_module_clear_elf(dt_module_t *dmp)
{
	free(dmp->dm_ctdata_data);
	dmp->dm_ctdata_data = NULL;

	memset(&dmp->dm_ctdata, 0, sizeof(ctf_sect_t));
	for (int i = 0; i < 2; i++) {
		dt_module_symbols_t *syms = &dmp->dm_symbols[i];

		free(syms->dm_symbuckets);
		free(syms->dm_symchains);
		free(syms->dm_asmap);
		memset(syms, 0, sizeof(*syms));
	}

	elf_end(dmp->dm_elf);
	dmp->dm_elf = NULL;

	dmp->dm_flags &= ~DT_DM_LOADED;
}

/* Release module metadata only when its owning module is destroyed. */
static void
dt_module_destroy(dtrace_hdl_t *dtp, dt_module_t *dmp)
{
	dt_btf_destroy(dtp, dmp->dm_btf);
	ctf_close(dmp->dm_ctfp);
	dt_module_clear_elf(dmp);
	dt_symtab_destroy(dtp, dmp->dm_kernsyms);
	free(dmp->dm_text_addrs);
	free(dmp->dm_data_addrs);
	dt_idhash_destroy(dmp->dm_extern);
}

/*
 * Insert a new external symbol reference into the specified module.  The new
 * symbol will be marked as undefined and is assigned a symbol index beyond
 * any existing cached symbols from this module.  We use the ident's di_data
 * field to store a pointer to a copy of the dtrace_syminfo_t for this symbol.
 */
dt_ident_t *
dt_module_extern(dtrace_hdl_t *dtp, dt_module_t *dmp,
    const char *name, const dtrace_typeinfo_t *tip)
{
	dtrace_syminfo_t *sip;
	dt_ident_t *idp;
	uint_t id;

	if (dmp->dm_extern == NULL && (dmp->dm_extern = dt_idhash_create(
	    "extern", NULL,
	    dmp->dm_symbols[0].dm_nsymelems +
	    dmp->dm_symbols[1].dm_nsymelems, UINT_MAX)) == NULL) {
		dt_set_errno(dtp, EDT_NOMEM);
		return NULL;
	}

	if (dt_idhash_nextid(dmp->dm_extern, &id) == -1) {
		dt_set_errno(dtp, EDT_SYMOFLOW);
		return NULL;
	}

	if ((sip = malloc(sizeof(dtrace_syminfo_t))) == NULL) {
		dt_set_errno(dtp, EDT_NOMEM);
		return NULL;
	}

	idp = dt_idhash_insert(dmp->dm_extern, name, DT_IDENT_SYMBOL, 0, id,
	    _dtrace_symattr, 0, &dt_idops_thaw, NULL, dtp->dt_gen);

	if (idp == NULL) {
		dt_set_errno(dtp, EDT_NOMEM);
		free(sip);
		return NULL;
	}

	sip->object = dmp->dm_name;
	sip->name = idp->di_name;
	sip->id = idp->di_id;

	idp->di_data = sip;
	idp->di_ctfp = tip->dtt_ctfp;
	idp->di_type = tip->dtt_type;

	return idp;
}

const char *
dt_module_modelname(dt_module_t *dmp)
{
	if ((dmp->dm_ops == &dt_modops_64) || (dmp->dm_ops == NULL))
		return "64-bit";
	else
		return "32-bit";
}

/*
 * Exported interface to compare a GElf_Addr to an address range member, for
 * bsearch()ing.
 */
int
dtrace_addr_range_cmp(const void *addr_, const void *range_)
{
	const GElf_Addr *addr = addr_;
	const dtrace_addr_range_t *range = range_;

	if (*addr < range->dar_va)
		return -1;

	if (*addr >= range->dar_va + range->dar_size)
		return 1;

	return 0;
}

/*
 * Expand an address range and return the new entry.
 */
static dtrace_addr_range_t *
dtrace_addr_range_grow(dt_module_t *dmp, int is_text)
{
	dtrace_addr_range_t **range;
	dtrace_addr_range_t *new_range;
	dtrace_addr_range_t *final_range;
	size_t *size, *capacity;

	if (is_text) {
		range = &dmp->dm_text_addrs;
		size = &dmp->dm_text_addrs_size;
		capacity = &dmp->dm_text_addrs_cap;
	} else {
		range = &dmp->dm_data_addrs;
		size = &dmp->dm_data_addrs_size;
		capacity = &dmp->dm_data_addrs_cap;
	}

	if (*size == *capacity) {
		size_t cap = *capacity == 0 ? 4 : *capacity * 2;

		if (cap < *capacity || cap > SIZE_MAX / sizeof(**range))
			return NULL;
		new_range = realloc(*range, sizeof(**range) * cap);
		if (new_range == NULL)
			return NULL;

		*range = new_range;
		*capacity = cap;
	}
	new_range = *range;
	final_range = new_range + (*size);
	final_range->dar_va = 0;
	final_range->dar_size = 0;

	(*size)++;

	return final_range;
}

/*
 * Transform an nm(1)-style type field into an ELF info character.  This is the
 * rough inverse of code in nm(1) and kernel/module.c:elf_type().  (Extreme
 * accuracy is hardly called for in this application.)
 */
static char
sym_type_to_info(char info)
{
	int local = islower(info);
	int binding = local ? STB_LOCAL : STB_GLOBAL;
	int type;
	char lowinfo = tolower(info);

	switch (lowinfo) {
	case 't':
		type = STT_FUNC; break;
	case 'w':
		binding = STB_WEAK;
		type = STT_FUNC;
		break;
	case 'v':
		binding = STB_WEAK;
		type = STT_OBJECT;
		break;
	case 'a': /* a sort of data */
	case 'r':
	case 'g':
	case 'd':
	case 's':
	case 'b':
		type = STT_OBJECT; break;
	case 'c':
		type = STT_COMMON; break;

	case 'u': /* highly unlikely, kludge it */
	case '?':
	case 'n':
	default:
		type = STT_NOTYPE; break;
	}

	return GELF_ST_INFO(binding, type);
}

/*
 * Do all necessary post-creation initialization of a module of type
 * DT_DM_KERNEL.
 */
static int
dt_kern_module_init(dtrace_hdl_t *dtp, dt_module_t *dmp)
{
	dt_dprintf("initializing module %s\n", dmp->dm_name);

	dmp->dm_ops = NULL;
	dmp->dm_text_addrs = NULL;
	dmp->dm_data_addrs = NULL;
 	dmp->dm_text_addrs_size = 0;
	dmp->dm_data_addrs_size = 0;
	dmp->dm_flags |= DT_DM_KERNEL;

	return 0;
}

#ifdef HAVE_LIBCTF
static void
dt_kern_module_ctf_from_btf(dtrace_hdl_t *dtp, dt_module_t *dmp)
{
	assert(dmp->dm_btf != NULL);

	dt_dprintf("Generating CTF for module %s from BTF.\n", dmp->dm_name);

	dmp->dm_ctfp = dt_btf_module_ctf(dtp, dmp);
	if (dmp->dm_ctfp == NULL)
		dt_dprintf("Cannot generate CTF for module %s from BTF: %s; "
			   "looking for out-of-tree module.\n",
			   dmp->dm_name, ctf_errmsg(dtp->dt_ctferr));
}
#endif

/*
 * Load BTF data for a kernel module (vmlinux is treated as its own module as
 * well), if available and not already loaded.
 */
static void
dt_kern_module_find_btf(dtrace_hdl_t *dtp, dt_module_t *dmp)
{
	/* The vmlinux module nust already exist as dtp->dt_exec. */
	assert(dtp->dt_exec != NULL);

	/*
	 * If there is no shared BTF data, and we're trying to load BTF data
	 * for any module other than vmlinux, we can conclude no module has
	 * valid BTF data.
	 */
	if (dtp->dt_exec->dm_btf == NULL && dmp != dtp->dt_exec)
		return;

	dt_dprintf("Loading BTF for module %s.\n", dmp->dm_name);

	/*
	 * If no module specific BTF is found, the kernel either does not
	 * provide any BTF data or something went wrong.  Neither is fatal.
	 */
	dt_btf_load_module(dtp, dmp);
}

/*
 * Determine the location of a kernel module's CTF data.
 *
 * If the module is a CTF archive, also load it in.
 */
static void
dt_kern_module_find_ctf(dtrace_hdl_t *dtp, dt_module_t *dmp)
{
	dt_module_t *shared = dt_module_create(dtp, "shared_ctf");

	if (shared == NULL) {
		dt_set_errno(dtp, EDT_NOMEM);
		return;
	}

	/*
	 * Module not already known as a kernel module?  It must be an unloaded
	 * one, since we do not support userspace modules yet.
         */
	if (!(dmp->dm_flags & DT_DM_KERNEL)) {
		dmp->dm_flags |= DT_DM_KERNEL;
		dmp->dm_flags |= DT_DM_KERN_UNLOADED;
		dt_kern_module_init(dtp, dmp);
	}

	/*
	 * Kernel modules' CTF can be in one of two places: the CTF archive or
	 * linked into the module (for out-of-tree modules).  The corresponding
	 * dt_module for the shared CTF, wherever found, is named 'shared_ctf'.
	 *
	 * Note: before we call this function we cannot distinguish between a
	 * non-loaded kernel module and a userspace module.  Neither have
	 * DT_DM_KERNEL turned on: the only difference is that the latter has no
	 * entry in the kernpath hash.
	 *
	 * Modules in the CTF archive are simpler: they just pull their CTF
	 * straight out of dt_ctfa, as needed.
	 *
	 * Check for a CTF archive containing the specified module.
	 */
	if (dtp->dt_ctfa == NULL && shared->dm_ctfp == NULL) {
		char *ctfa_name;
		char *to;

		/* use user provided CTF archive. */
		if (dtp->dt_ctfa_path == NULL) {
			ctfa_name = malloc(strlen(dtp->dt_module_path) +
			    strlen("/kernel/vmlinux.ctfa") + 1);
			if (ctfa_name == NULL) {
				dt_set_errno(dtp, EDT_NOMEM);
				return;
			}
			to = stpcpy(ctfa_name, dtp->dt_module_path);
			stpcpy(to, "/kernel/vmlinux.ctfa");
		} else
			ctfa_name = dtp->dt_ctfa_path;

		/* Cache absence, but retry allocation, permission, and I/O errors. */
		if (dtp->dt_ctfa_missing)
			dtp->dt_ctferr = dtp->dt_ctfa_missing;
		else {
#ifdef HAVE_LIBCTF
			dtp->dt_ctfa = dt_ctfa_open(dtp, ctfa_name);
#else
			dtp->dt_ctfa = ctf_arc_open(ctfa_name, &dtp->dt_ctferr);
#endif
			if (dtp->dt_ctfa == NULL &&
			    (dtp->dt_ctferr == ENOENT || dtp->dt_ctferr == ENOTDIR))
				dtp->dt_ctfa_missing = dtp->dt_ctferr;
		}
		if (dtp->dt_ctfa != NULL) {
			/*
			 * Load in the shared CTF immediately.
			 */
			shared->dm_ctfp = ctf_arc_open_by_name(dtp->dt_ctfa,
			    "shared_ctf", &dtp->dt_ctferr);

			if (shared->dm_ctfp == NULL) {
				dt_dprintf("Cannot get shared CTF from archive %s: %s.",
				    ctfa_name, ctf_errmsg(dtp->dt_ctferr));
				ctf_arc_close(dtp->dt_ctfa);
				dtp->dt_ctfa = NULL;
				free(dtp->dt_ctfa_data);
				dtp->dt_ctfa_data = NULL;
			} else {
				shared->dm_flags |= DT_DM_LOADED | DT_DM_CTF_ARCHIVED;
				ctf_setspecific(shared->dm_ctfp, shared);
				dt_dprintf("Loaded shared CTF from archive %s.\n",
				    ctfa_name);
			}
		} else {
#ifdef HAVE_LIBCTF
			dt_module_t	*mod;

			dt_dprintf("Cannot open CTF archive %s: %s; "
				   "trying BTF.\n",
				   ctfa_name, ctf_errmsg(dtp->dt_ctferr));

			/*
			 * Try to load the vmlinux BTF data to generate the CTF
			 * data for shared_ctf from.
			 */
			mod = dt_module_lookup_by_name(dtp, "vmlinux");
			if (mod != NULL && mod->dm_btf == NULL)
				dt_btf_load_module(dtp, mod);
			if (mod != NULL && mod->dm_btf != NULL) {
				dt_kern_module_ctf_from_btf(dtp, mod);
			} else
				dt_dprintf("No BTF data for vmlinux; "
					   "looking for in-module CTF instead.\n");
#else
			dt_dprintf("Cannot open CTF archive %s: %s; "
				   "looking for in-module CTF instead.\n",
				   ctfa_name, ctf_errmsg(dtp->dt_ctferr));
#endif
		}

		if (dtp->dt_ctfa_path == NULL)
			free(ctfa_name);
	}

	/* The shared module already owns the archive/BTF parent dictionary. */
	if (dmp == shared && shared->dm_ctfp != NULL)
		return;

	/* Older kernels carry the shared dictionary in the ctf module. */
	if (dmp != shared && shared->dm_ctfp == NULL)
		dt_module_getctf(dtp, shared);

	if (dtp->dt_ctfa != NULL) {
		dt_dprintf("Loading CTF for module %s from archive.\n",
		    dmp->dm_name);

		dmp->dm_ctfp = ctf_arc_open_by_name(dtp->dt_ctfa,
		    dmp->dm_name, &dtp->dt_ctferr);

		if (dmp->dm_ctfp == NULL) {
			dt_dprintf("Cannot open CTF for module %s in CTF "
			    "archive: %s; looking for out-of-tree module.\n",
			    dmp->dm_name, ctf_errmsg(dtp->dt_ctferr));
		}
	}

	if (dmp->dm_ctfp != NULL) {
		const char *parent;

		/*
		 * Initialize other stuff around the CTF.  Much of it
		 * (e.g. dm_ctdata_*) is not populated, but some stuff still
		 * needs to be done.
		 *
		 * We do not yet support parent CTF that is not the shared CTF
		 * from the CTF archive; this support will only ever be needed
		 * for userspace CTF, and for that we can always get the CTF
		 * from the same archive as the child dict's in any case (and it
		 * is done for us by libctf).
		 *
		 * Don't even try to keep CTF around if parent importing fails:
		 * CTF with its parent types sliced off is useless.
		 */
		if ((parent = ctf_parent_name(dmp->dm_ctfp)) != NULL) {
			assert(strcmp(parent, "shared_ctf") == 0);
			if (ctf_import(dmp->dm_ctfp, dt_module_shared_ctf(dtp)) < 0) {
				dt_dprintf("Importing of CTF for %s "
				    "failed: %s\n", dmp->dm_name,
				    ctf_errmsg(ctf_errno(dmp->dm_ctfp)));
				dtp->dt_ctferr = ctf_errno(dmp->dm_ctfp);
				ctf_close(dmp->dm_ctfp);
				dmp->dm_ctfp = NULL;
				return;
			}
			dt_dprintf("loaded CTF container for %s (%p)\n",
			    dmp->dm_name, (void *)dmp->dm_ctfp);
		}

		if (dtp->dt_ctfa != NULL)
			dmp->dm_flags |= DT_DM_CTF_ARCHIVED;
		ctf_setspecific(dmp->dm_ctfp, dmp);
#ifdef HAVE_LIBCTF
	} else if (dmp->dm_btf != NULL) {
		/* Generate CTF from BTF for the module. */
		dt_kern_module_ctf_from_btf(dtp, dmp);
	} else
		dt_dprintf("No BTF data for %s; "
			   "looking for in-module CTF instead.\n",
			   dmp->dm_name);
#else
	}
#endif

	/*
	 * No CTF archive, module not present in it, or module not loaded so
	 * we'll need its symbol table later on.  Check for a standalone module.
	 */
	if ((dmp->dm_ctfp == NULL) || (dmp->dm_flags & DT_DM_KERN_UNLOADED)) {
		dt_kern_path_t *dkpp = NULL;

		dkpp = dt_kern_path_lookup_by_name(dtp,
		    dmp == shared ? "ctf" : dmp->dm_name);

		/*
		 * Not found, or not a kernel module at all?  That's quite
		 * acceptable: just return.
		 */
		if (!dkpp || !(dmp->dm_flags & DT_DM_KERNEL))
			return;

		strlcpy(dmp->dm_file, dkpp->dkp_path, sizeof(dmp->dm_file));
	}
}

#define PROBE_LIST		TRACEFS "available_filter_functions"

/*
 * Determine which kernel functions are traceable and mark them.
 */
void
dt_modsym_mark_traceable(dtrace_hdl_t *dtp)
{
	FILE			*f;
	char			*buf = NULL;
	size_t			len = 0;

	if (dt_symtab_traceable(dtp->dt_exec->dm_kernsyms))
		return;

	f = fopen(PROBE_LIST, "r");
	if (f == NULL)
		return;

	while (getline(&buf, &len, f) >= 0) {
		char			*p;
		dt_symbol_t		*sym = NULL;

		/*
		 * Here buf is either "funcname\n" or "funcname [modname]\n".
		 * The last line may not have a linefeed.
		 */
		p = strchr(buf, '\n');
		if (p) {
			*p = '\0';
			if (p > buf && *(--p) == ']')
				*p = '\0';
		}

		/* Now buf is either "funcname" or "funcname [modname". */
		p = strchr(buf, ' ');
		if (p) {
			*p++ = '\0';
			if (*p == '[')
				p++;
		}

#define strstarts(var, x) (strncmp(var, x, strlen (x)) == 0)
		/* Weed out __ftrace_invalid_address___* entries. */
		if (strstarts(buf, "__ftrace_invalid_address__") ||
		    strstarts(buf, "__probestub_") ||
		    strstarts(buf, "__traceiter_"))
			continue;
#undef strstarts

		/*
		 * If we have a module name, look for the symbol in that
		 * module.
		 * If not, perform a general symbol lookup to find its first
		 * instance.
		 */
		if (p) {
			dt_module_t	*dmp = dt_module_lookup_by_name(dtp, p);

			if (dmp)
				sym = dt_module_symbol_by_name(dtp, dmp, buf);
		} else
			sym = dt_symbol_by_name(dtp, buf);

		if (sym)
			dt_symbol_set_traceable(sym);
	}

	free(buf);
	fclose(f);

	dt_symtab_set_traceable(dtp->dt_exec->dm_kernsyms);
}

/*
 * Symbol data can be collected in three ways:
 *  - kallmodsyms
 *  - kallsyms with modules.builtin.ranges
 *  - kallsyms w/o modules.builtin.ranges
 *
 * The processing state uses a kind member to identify the primary source
 * (kallsyms vs kallmodsyms), and the rfp member is used to determine whether
 * a modules.builtin.ranges file is available.
 */
#define DT_MODSYM_KALLSYMS	1
#define DT_MODSYM_KALLMODSYMS	2

/*
 * We will use kernel_flag to track which symbols we are reading.
 *
 * /proc/kall[mod]syms starts with kernel (and built-in-module) symbols.
 *
 * The last kernel address is expected to have the name "_end",
 * but there might also be a symbol "__brk_limit" with that address.
 * Set the KERNEL_FLAG_KERNEL_END flag while these addresses are read.
 *
 * Otherwise, symbols in /proc/kallmodsyms will normally belong to
 * loadable modules.  Set the KERNEL_FLAG_LOADABLE flag once these
 * symbols are reached.
 *
 * Another odd case is the .init.scratch section introduced by e1bfa87
 * ("x86/mm: Create a workarea in the kernel for SME early encryption"),
 * which appears in 5.2-rc6.  We ignore this section by setting the
 * KERNEL_FLAG_INIT_SCRATCH flag.
 */
#define KERNEL_FLAG_KERNEL_END 1
#define KERNEL_FLAG_LOADABLE 2
#define KERNEL_FLAG_INIT_SCRATCH 4

typedef struct dt_kasstate {
	GElf_Addr	sect_base;
	char		sect_symb[PATH_MAX];
	GElf_Addr	sect_off;

	GElf_Addr	mod_saddr, mod_eaddr;
	char		mod_name[PATH_MAX];
	FILE		*rfp;
	char		*line;
	size_t		line_n;
	int		kind;

	int		kernel_flag;
	int		last_sym_text;
	dt_module_t	*last_dmp;
	dt_module_t	*lookup_dmp;
	char		lookup_name[DTRACE_MODNAMELEN];
} dt_kasstate_t;

static dt_kasstate_t *
dt_kasstate_new(dtrace_hdl_t *dtp, int kind)
{
	char		*fn;
	dt_kasstate_t	*state;

	state = dt_zalloc(dtp, sizeof(dt_kasstate_t));
	if (state == NULL)
		return NULL;
	state->kind = kind;
	state->kernel_flag = 0;
	state->last_dmp = NULL;
	state->last_sym_text = -1;

	if (kind == DT_MODSYM_KALLMODSYMS)
		return state;

	if (asprintf(&fn, "%s/modules.builtin.ranges",
		     dtp->dt_module_path) == -1)
		return state;

	state->rfp = fopen(fn, "r");
	free(fn);

	if (state->rfp == NULL) {
		/* TODO: waiting on a warning infrastructure */
		dt_dprintf("warning: unable to read "
			   "%s/modules.builtin.ranges\n",
			   dtp->dt_module_path);
	}

	return state;
}

static char *
dt_kasstate_modname(dt_kasstate_t *state, GElf_Addr addr, const char *name)
{
	char	*line;

	/* Nothing to be done if there is no modules.builtin.ranges file. */
	if (state->rfp == NULL)
		return NULL;

	/*
	 * If we are looking for an anchor symbol, see if the current symbol is
	 * the one we want.  If so, update the section base address and clear
	 * the symbol so we don't look for it again.  If not, we are done
	 * because the current address cannot be part of the last module we saw
	 * once we are looking for the next section anchor.
	 */
	if (state->sect_symb[0]) {
		if (strcmp(state->sect_symb, name) != 0)
			return NULL;

		state->sect_base = addr - state->sect_off;
		state->sect_symb[0] = 0;
	}

	/*
	 * If the given address is beyond the end of the current builtin module
	 * more data needs to be read from modules.builtin.ranges until a range
	 * is found that either contains the address or goes beyond.
	 */
	while (addr > state->mod_eaddr) {
		if (getline(&state->line, &state->line_n, state->rfp) == -1)
			break;

		line = state->line;
		if (sscanf(line, "%*s %lx-%lx %[A-Za-z0-9_ ]",
			   &state->mod_saddr, &state->mod_eaddr,
			   state->mod_name) == 3) {
			state->mod_saddr += state->sect_base;
			state->mod_eaddr += state->sect_base;
			state->mod_name[strlen(state->mod_name)] = '\0';
		} else {
			if (sscanf(line, "%*s %lx-%*x = %[A-Za-z0-9_]",
				   &state->sect_off, state->sect_symb) < 2)
				dt_dprintf("malformed modules.builtin.ranges line: %s\n", line);
			return NULL;
		}
	}

	if (addr >= state->mod_saddr && addr < state->mod_eaddr)
		return state->mod_name;

	return NULL;
}

typedef struct dt_kallsym {
	GElf_Addr	addr;
	size_t		size;
	char		type;
	char		name[KSYM_NAME_MAX];
	char		mod[PATH_MAX];
} dt_kallsym_t;

static int
dt_modsym_addsym(dtrace_hdl_t *dtp, dt_module_t *dmp, dt_kallsym_t *sym,
		 dt_kasstate_t *state)
{
	int			sym_text;
	dtrace_addr_range_t	*range = NULL;
	int			skip = 0;

	if (dtp->dt_modranges_valid)
		dt_module_ranges_invalidate(dtp);

	/*
	 * Add the symbol to the module's kernel symbol table.
	 *
	 * Some very voluminous and unuseful symbols are silently skipped, being
	 * used to update ranges but not added to the kernel symbol table.
	 * It doesn't matter much if this net is cast too wide, since we only
	 * care if a symbol is present if control flow or data lookups might
	 * pass through it while a probe fires, and that won't happen to any
	 * of these symbols.
	 */
#define strstarts(var, x) (strncmp(var, x, strlen (x)) == 0)
	if ((strstarts(sym->name, "__crc_")) ||
	    (strstarts(sym->name, "__ksymtab_")) ||
	    (strstarts(sym->name, "__kcrctab_")) ||
	    (strstarts(sym->name, "__kstrtab_")) ||
	    (strstarts(sym->name, "__param_")) ||
	    (strstarts(sym->name, "__syscall_meta__")) ||
	    (strstarts(sym->name, "__p_syscall_meta__")) ||
	    (strstarts(sym->name, "__event_")) ||
	    (strstarts(sym->name, "event_") && sym->type == 'd') ||
	    (strstarts(sym->name, "ftrace_event_")) ||
	    (strstarts(sym->name, "types__")) ||
	    (strstarts(sym->name, "args__")) ||
	    (strstarts(sym->name, "__tracepoint_")) ||
	    (strstarts(sym->name, "__tpstrtab_")) ||
	    (strstarts(sym->name, "__tpstrtab__")) ||
	    (strstarts(sym->name, "__initcall_")) ||
	    (strstarts(sym->name, "__pci_fixup_")))
		skip = 1;
#undef strstarts

	if (!skip) {
		if (dmp->dm_kernsyms == NULL)
			dmp->dm_kernsyms = dt_symtab_create(dtp);

		if (dmp->dm_kernsyms == NULL)
			return EDT_NOMEM;

		if (dt_symbol_insert(dtp, dmp->dm_kernsyms, dmp, sym->name,
				     sym->addr, sym->size,
				     sym_type_to_info(sym->type)) == NULL)
			return EDT_NOMEM;
	}

	/*
	 * Expand the appropriate address range for this module.
	 * Picking the right range is easy, but done differently
	 * for loadable modules versus built-in modules (and kernel).
	 */

	if (sym->size == 0)
		return 0;

	sym_text = (sym->type == 't') || (sym->type == 'T') ||
		   (sym->type == 'w') || (sym->type == 'W');

	if ((state->kernel_flag & KERNEL_FLAG_LOADABLE) == 0) {
		/*
		 * The kernel and built-in modules are in address order.
		 */
		if (dmp == state->last_dmp &&
		    sym_text == state->last_sym_text) {
			if (sym_text)
				range = &dmp->dm_text_addrs
				    [dmp->dm_text_addrs_size - 1];
			else
				range = &dmp->dm_data_addrs
				    [dmp->dm_data_addrs_size - 1];
			range->dar_size = sym->addr + sym->size - range->dar_va;
		} else {
			state->last_dmp = dmp;
			state->last_sym_text = sym_text;
		}
	} else {
		/*
		 * Each loadable module has only one text and one data range.
		 */

		if (sym_text)
			range = dmp->dm_text_addrs;
		else
			range = dmp->dm_data_addrs;

		if (range) {
			GElf_Addr end;
			end = range->dar_va + range->dar_size;
			if (range->dar_va > sym->addr)
				range->dar_va = sym->addr;
			if (end < sym->addr + sym->size)
				end = sym->addr + sym->size;
			range->dar_size = end - range->dar_va;
		}

	}

	/*
	 * If there is no range to expand, create a new one.
	 */

	if (range == NULL) {
		range = dtrace_addr_range_grow(dmp, sym_text);
		if (range == NULL)
			return EDT_NOMEM;
		range->dar_va = sym->addr;
		range->dar_size = sym->size;
	}

	return 0;
}

/* Parse a hexadecimal field without scanf's general format machinery. */
static int
dt_modsym_hex(const char **line, uint64_t *value)
{
	const char *p = *line;
	uint64_t v = 0;
	int digits = 0;

	while (isspace((unsigned char)*p))
		p++;
	for (;;) {
		unsigned int n;

		if (*p >= '0' && *p <= '9')
			n = *p - '0';
		else if (*p >= 'a' && *p <= 'f')
			n = *p - 'a' + 10;
		else if (*p >= 'A' && *p <= 'F')
			n = *p - 'A' + 10;
		else
			break;
		if (v > (UINT64_MAX - n) / 16)
			return -1;
		v = v * 16 + n;
		p++;
		digits++;
	}
	if (!digits || !isspace((unsigned char)*p))
		return -1;
	*value = v;
	*line = p;
	return 0;
}

static int
dt_modsym_parse(const char *line, int kind, dt_kallsym_t *sym)
{
	const char *end;
	uint64_t value;
	size_t len;

	if (dt_modsym_hex(&line, &value) != 0)
		return -1;
	sym->addr = value;
	sym->size = 1;
	if (kind == DT_MODSYM_KALLMODSYMS) {
		if (dt_modsym_hex(&line, &value) != 0 || value > SIZE_MAX)
			return -1;
		sym->size = value;
	}
	while (isspace((unsigned char)*line))
		line++;
	if (*line == '\0')
		return -1;
	sym->type = *line++;
	if (!isspace((unsigned char)*line))
		return -1;
	while (isspace((unsigned char)*line))
		line++;
	len = strcspn(line, " \t\r\n");
	if (len == 0 || len >= sizeof(sym->name))
		return -1;
	memcpy(sym->name, line, len);
	sym->name[len] = '\0';
	line += len;
	while (isspace((unsigned char)*line))
		line++;
	if (*line == '\0') {
		strcpy(sym->mod, "vmlinux");
		return 0;
	}
	if (*line++ != '[' || (end = strchr(line, ']')) == NULL)
		return -1;
	len = end - line;
	if (len == 0 || len >= sizeof(sym->mod))
		return -1;
	memcpy(sym->mod, line, len);
	sym->mod[len] = '\0';
	return 1;		/* Explicit module name(s). */
}

/*
 * Update our module cache.  For each line, create or
 * populate the dt_module_t for this module (if necessary), extend its address
 * ranges as needed, and add the symbol in this line to the module's kernel
 * symbol table.
 *
 * Modules discovered by an earlier update retain their symbols and ranges.
 */
static int
dt_modsym_update(dtrace_hdl_t *dtp, const char *line, dt_kasstate_t *state)
{
	dt_kallsym_t	sym;
	char		*modname;

	if ((line[0] == '\n') || (line[0] == 0))
		return 0;

	{
		int explicit_module = dt_modsym_parse(line, state->kind, &sym);

		if (explicit_module < 0) {
			dt_dprintf("malformed kernel symbol line: %s\n", line);
			return EDT_CORRUPT_KALLSYMS;
		}
		if (state->kind == DT_MODSYM_KALLSYMS && !explicit_module &&
		    !(state->kernel_flag & KERNEL_FLAG_LOADABLE)) {
			char *s = dt_kasstate_modname(state, sym.addr, sym.name);

			if (s != NULL)
				strlcpy(sym.mod, s, sizeof(sym.mod));
		}
	}

	if (strcmp(sym.mod, "bpf") == 0)
		return 0;

	/*
	 * "__builtin__kprobes" is used as a module name for symbols for pages
	 * allocated for kprobes' purposes, even though it is not a module.
	 */
	if (strcmp(sym.mod, "__builtin__kprobes") == 0)
		return 0;

	/*
	 * Symbols of "absolute" type are typically defined per CPU.
	 * Their "addresses" here are very low and are actually offsets.
	 * Drop these symbols.
	 */
	if ((sym.type == 'a') || (sym.type == 'A'))
		return 0;

	/*
	 * Skip over the .init.scratch section.
	 */
	if (strcmp(sym.name, "__init_scratch_begin") == 0) {
		state->kernel_flag |= KERNEL_FLAG_INIT_SCRATCH;
		return 0;
	} else if (strcmp(sym.name, "__init_scratch_end") == 0) {
		state->kernel_flag &= ~ KERNEL_FLAG_INIT_SCRATCH;
		return 0;
	} else if (state->kernel_flag & KERNEL_FLAG_INIT_SCRATCH) {
		return 0;
	}

	if ((strcmp(sym.name, "_end") == 0) ||
	    (strcmp(sym.name, "__brk_limit") == 0))
		state->kernel_flag |= KERNEL_FLAG_KERNEL_END;
	else if (state->kernel_flag & KERNEL_FLAG_KERNEL_END)
		state->kernel_flag = KERNEL_FLAG_LOADABLE;

	/*
	 * Special case: rename the 'ctf' module to 'shared_ctf': the
	 * parent-name lookup code presumes that names that appear in CTF's
	 * parent section are the names of modules, but the ctf module's CTF
	 * section is special-cased to contain the contents of the shared_ctf
	 * repository, not ctf.ko's types.
	 */
	if (strcmp(sym.mod, "ctf") == 0)
		strcpy(sym.mod, "shared_ctf");

	/*
	 * It is possible that the symbol belongs to more than one module.
	 * Loop over all modules in sym.mod and add the symbol to each.
	 */
	for (modname = strtok(sym.mod, " "); modname;
	     modname = strtok(NULL, " ")) {
		dt_module_t	*dmp;
		int		err;

		/* Separate from last_dmp, which tracks address-range adjacency. */
		if (state->lookup_dmp && strcmp(state->lookup_name, modname) == 0)
			dmp = state->lookup_dmp;
		else
			dmp = dt_module_lookup_by_name(dtp, modname);
		if (dmp == NULL) {
			dmp = dt_module_create(dtp, modname);
			if (dmp == NULL)
				return EDT_NOMEM;
		}

		state->lookup_dmp = dmp;
		strlcpy(state->lookup_name, modname, sizeof(state->lookup_name));

		if (!(dmp->dm_flags & DT_DM_KERNEL)) {
			err = dt_kern_module_init(dtp, dmp);
			if (err != 0)
				return err;
		}

		/* Never replace metadata for a previously discovered module. */
		if (dmp->dm_flags & DT_DM_KERN_SYMS)
			continue;

		/* A module previously known only through on-disk types is now live. */
		dmp->dm_flags &= ~DT_DM_KERN_UNLOADED;

		err = dt_modsym_addsym(dtp, dmp, &sym, state);
		if (err != 0)
			return err;
	}

	return 0;
}
#undef KERNEL_FLAG_KERNEL_END
#undef KERNEL_FLAG_LOADABLE
#undef KERNEL_FLAG_INIT_SCRATCH

/*
 * Discover new kernel modules without invalidating existing symbols, address
 * ranges, or types.  Module unloading and reloading are not supported: the first
 * discovered instance of a module is retained until the handle is closed.
 */
int
dtrace_update(dtrace_hdl_t *dtp)
{
	dt_module_t	*dmp;
	FILE		*fd;
	dt_kasstate_t	*state = NULL;

	/*
	 * Note all the symbols currently loaded into the kernel's address
	 * space and construct modules with appropriate address ranges from
	 * each.
	 */
	if ((fd = fopen("/proc/kallmodsyms", "r")) != NULL)
		state = dt_kasstate_new(dtp, DT_MODSYM_KALLMODSYMS);
	else if ((fd = fopen("/proc/kallsyms", "r")) != NULL)
		state = dt_kasstate_new(dtp, DT_MODSYM_KALLSYMS);

	if (fd != NULL && state == NULL) {
		fclose(fd);
		return dt_set_errno(dtp, EDT_NOMEM);
	}

	if (fd != NULL) {
		char *line = NULL;
		size_t line_n = 0;
		while ((getline(&line, &line_n, fd)) > 0)
			if (dt_modsym_update(dtp, line, state) != 0) {
				/* TODO: waiting on a warning infrastructure */
				dt_dprintf("warning: module CTF loading failed"
				    " on %s line %s\n",
				    state->kind == DT_MODSYM_KALLSYMS
					? "kallsyms" : "kallmodsyms", line);
				break; /* no hope of (much) CTF */
			}
		free(line);
		fclose(fd);
	} else {
		/* TODO: waiting on a warning infrastructure */
		dt_dprintf("warning: /proc/kallmodsyms is not "
		    "present: consider setting -x procfspath\n");
		dt_dprintf("warning: module CTF loading failed\n");
	}

	/*
	 * Work over all modules, now they are (hopefully) fully populated.
	 */
	for (dmp = dt_list_next(&dtp->dt_modlist); dmp != NULL;
	    dmp = dt_list_next(dmp)) {
		if (!(dmp->dm_flags & DT_DM_KERNEL) ||
		    (dmp->dm_flags & (DT_DM_KERN_SYMS | DT_DM_KERN_UNLOADED)))
			continue;

		if (dmp->dm_kernsyms != NULL) {
			dt_symtab_sort(dmp->dm_kernsyms,
				       state->kind == DT_MODSYM_KALLSYMS);
			dt_symtab_pack(dmp->dm_kernsyms);
		}
		dmp->dm_flags |= DT_DM_KERN_SYMS;
	}

	/* Discover BTF IDs for new modules, retaining IDs already cached. */
	dt_btf_get_module_ids(dtp);

	/*
	 * Look up all the macro identifiers and set di_id to the latest value.
	 * This code collaborates with dt_lex.l on the use of di_id.  We will
	 * need to implement something fancier if we need to support non-ints.
	 */
	dt_idhash_lookup(dtp->dt_macros, "egid")->di_id = getegid();
	dt_idhash_lookup(dtp->dt_macros, "euid")->di_id = geteuid();
	dt_idhash_lookup(dtp->dt_macros, "gid")->di_id = getgid();
	dt_idhash_lookup(dtp->dt_macros, "pid")->di_id = getpid();
	dt_idhash_lookup(dtp->dt_macros, "pgid")->di_id = getpgid(0);
	dt_idhash_lookup(dtp->dt_macros, "ppid")->di_id = getppid();
	dt_idhash_lookup(dtp->dt_macros, "sid")->di_id = getsid(0);
	dt_idhash_lookup(dtp->dt_macros, "uid")->di_id = getuid();

	/*
	 * Cache the pointers to the module representing the shared kernel
	 * symbols in the dtrace client handle.  (This is the same as 'genunix'
	 * in Solaris.)
	 */
	dtp->dt_exec = dt_module_lookup_by_name(dtp, "vmlinux");

	/*
	 * If this is the first time we are initializing the module list,
	 * shuffle the modules for vmlinux and the shared ctf to the front of
	 * the module list.  We do this so that type and symbol queries
	 * encounter vmlinux and thereby optimize for the common case in
	 * dtrace_lookup_by_name() and dtrace_lookup_by_type(), below.  The
	 * dtrace module is also shuffled to the front so that types used only,
	 * or largely, by DTrace translators and the DTrace module are resolved
	 * quickly.  (There are a few of these, mostly Solaris-compatibility
	 * types such as caddr_t.)
	 */
	if (dtp->dt_cdefs == NULL && dtp->dt_ddefs == NULL) {
		dt_module_shuffle_to_start(dtp, "dtrace");
		dt_module_shuffle_to_start(dtp, "vmlinux");
	}

	if (state) {
		if (state->rfp)
			fclose(state->rfp);

		free(state->line);
		dt_free(dtp, state);
	}

	return 0;
}

/*
 * Shuffle one module to the start of the module list.
 */
static void
dt_module_shuffle_to_start(dtrace_hdl_t *dtp, const char *name)
{
	dt_module_t *dmp = dt_module_lookup_by_name(dtp, name);

	if (!dmp)
		return;

	dt_list_delete(&dtp->dt_modlist, dmp);
	dt_list_prepend(&dtp->dt_modlist, dmp);
	dt_module_ranges_invalidate(dtp);
}

static dt_module_t *
dt_module_from_object(dtrace_hdl_t *dtp, const char *object)
{
	int err = EDT_NOMOD;
	dt_module_t *dmp;

	if (object == DTRACE_OBJ_EXEC)
		dmp = dtp->dt_exec;
	else if (object == DTRACE_OBJ_CDEFS)
		dmp = dtp->dt_cdefs;
	else if (object == DTRACE_OBJ_DDEFS)
		dmp = dtp->dt_ddefs;
	else {
		dmp = dt_module_create(dtp, object);
		err = EDT_NOMEM;
	}

	if (dmp == NULL)
		dt_set_errno(dtp, err);

	return dmp;
}

/*
 * Exported interface to look up a symbol by name.  We return the (possibly
 * partial) GElf_Sym and complete symbol information for the matching symbol.
 *
 * Only the st_info, st_value, and st_size fields of the GElf_Sym are guaranteed
 * to be populated: the st_shndx is populated but its only meaningful value is
 * SHN_UNDEF versus !SHN_UNDEF.
 */
int
dtrace_lookup_by_name(dtrace_hdl_t *dtp, const char *object, const char *name,
    GElf_Sym *symp, dtrace_syminfo_t *sip)
{
	dt_module_t *dmp;
	dt_ident_t *idp;
	uint_t n;
	GElf_Sym sym;

	uint_t mask = 0; /* mask of dt_module flags to match */
	uint_t bits = 0; /* flag bits that must be present */

	if (object != DTRACE_OBJ_EVERY &&
	    object != DTRACE_OBJ_KMODS &&
	    object != DTRACE_OBJ_UMODS) {
		if ((dmp = dt_module_from_object(dtp, object)) == NULL)
			return -1; /* dt_errno is set for us */

		if (dt_module_load(dtp, dmp) == -1)
			return -1; /* dt_errno is set for us */
		n = 1;

	} else {
		if (object == DTRACE_OBJ_KMODS)
			mask = bits = DT_DM_KERNEL;
		else if (object == DTRACE_OBJ_UMODS)
			mask = DT_DM_KERNEL;

		dmp = dt_list_next(&dtp->dt_modlist);
		n = dt_htab_entries(dtp->dt_mods);
	}

	if (symp == NULL)
		symp = &sym;

	/*
	 * Search loaded kernel symbols first, iff searching all kernel symbols
	 * is requested.  Fall back to per-module lookup for symbols loaded
	 * from ELF objects rather than the running kernel.
	 */
	if ((object == DTRACE_OBJ_EVERY || object == DTRACE_OBJ_KMODS) &&
	    dtp->dt_kernsyms) {
		dt_symbol_t *dt_symp;

		if ((dt_symp = dt_symbol_by_name(dtp, name)) != NULL) {
			if (sip != NULL) {
				dt_module_t *tmp;

				tmp = dt_symbol_module(dt_symp);
				sip->object = tmp->dm_name;
				sip->name = dt_symbol_name(dt_symp);
				sip->id = 0;	/* undefined */
			}

			dt_symbol_to_elfsym(dtp, dt_symp, symp);
			return 0;
		}
	}

	/*
	 * Not found: search all modules, including non-kernel modules, loading
	 * them as needed.
	 */
	for (; n > 0; n--, dmp = dt_list_next(dmp)) {
		if ((dmp->dm_flags & mask) != bits)
			continue; /* failed to match required attributes */

		if (dt_module_load(dtp, dmp) == -1)
			continue; /* failed to load symbol table */

		/*
		 * If this is a kernel symbol, see if it's appeared.  If it has,
		 * it must belong to this module.
		 */
		if ((dmp->dm_flags & DT_DM_KERNEL) &&
		    (!(dmp->dm_flags & DT_DM_KERN_UNLOADED))) {
			dt_symbol_t *dt_symp;

			if (!dmp->dm_kernsyms)
				continue;

			dt_symp = dt_module_symbol_by_name(dtp, dmp, name);
			if (!dt_symp)
				continue;
			assert(dt_symbol_module(dt_symp) == dmp);

			if (sip != NULL) {
				sip->object = dmp->dm_name;
				sip->name = dt_symbol_name(dt_symp);
				sip->id = 0;	/* undefined */
			}
			dt_symbol_to_elfsym(dtp, dt_symp, symp);

			return 0;
		} else {
			uint_t id;

			if (dt_module_symname(dmp, name, symp, &id) != NULL) {
				if (sip != NULL) {
					sip->object = dmp->dm_name;
					sip->name = dt_module_symstr(dmp, id, symp);
					sip->id = id;
				}
				return 0;
			}
		}

		if (dmp->dm_extern != NULL &&
		    (idp = dt_idhash_lookup(dmp->dm_extern, name)) != NULL) {
			if (symp != &sym) {
				symp->st_name = (uintptr_t)idp->di_name;
				symp->st_info =
				    GELF_ST_INFO(STB_GLOBAL, STT_NOTYPE);
				symp->st_other = 0;
				symp->st_shndx = SHN_UNDEF;
				symp->st_value = 0;
				symp->st_size =
				    ctf_type_size(idp->di_ctfp, idp->di_type);
			}

			if (sip != NULL) {
				sip->object = dmp->dm_name;
				sip->name = idp->di_name;
				sip->id = idp->di_id;
			}

			return 0;
		}
	}

	return dt_set_errno(dtp, EDT_NOSYM);
}

static int
dt_module_range_cmp(const void *a, const void *b)
{
	const dt_module_range_t *p = a, *q = b;

	return p->start < q->start ? -1 : p->start > q->start;
}

/* Build lazily: handles that never symbolize addresses need no index. */
static int
dt_module_ranges_build(dtrace_hdl_t *dtp)
{
	dt_module_t *dmp;
	dt_module_range_t *index;
	size_t total = 0, n = 0, order = 0, i;
	GElf_Addr max_last = 0;

	for (dmp = dt_list_next(&dtp->dt_modlist); dmp;
	     dmp = dt_list_next(dmp)) {
		if (dmp->dm_text_addrs_size > SIZE_MAX - total)
			return -1;
		total += dmp->dm_text_addrs_size;
		if (dmp->dm_data_addrs_size > SIZE_MAX - total)
			return -1;
		total += dmp->dm_data_addrs_size;
	}
	if (total > SIZE_MAX / sizeof(*index))
		return -1;
	index = total ? malloc(total * sizeof(*index)) : NULL;
	if (total && index == NULL)
		return -1;

	for (dmp = dt_list_next(&dtp->dt_modlist); dmp;
	     dmp = dt_list_next(dmp), order++) {
		int text;

		for (text = 0; text < 2; text++) {
			dtrace_addr_range_t *ranges = text ? dmp->dm_text_addrs :
							   dmp->dm_data_addrs;
			size_t size = text ? dmp->dm_text_addrs_size :
					    dmp->dm_data_addrs_size;

			for (i = 0; i < size; i++) {
				GElf_Addr start = ranges[i].dar_va;
				GElf_Addr len = ranges[i].dar_size;

				if (len == 0)
					continue;
				index[n].start = start;
				index[n].last = len - 1 > UINT64_MAX - start ?
						UINT64_MAX : start + len - 1;
				index[n].module = dmp;
				index[n++].order = order;
			}
		}
	}
	if (n)
		qsort(index, n, sizeof(*index), dt_module_range_cmp);
	for (i = 0; i < n; i++) {
		if (index[i].last > max_last)
			max_last = index[i].last;
		index[i].max_last = max_last;
	}
	dtp->dt_modranges = index;
	dtp->dt_modranges_count = n;
	dtp->dt_modranges_valid = 1;
	return 0;
}

dt_module_t *
dt_module_by_addr(dtrace_hdl_t *dtp, GElf_Addr addr)
{
	dt_module_range_t *index;
	dt_module_t *dmp = NULL;
	size_t lo = 0, hi, best = SIZE_MAX;

	if (!dtp->dt_modranges_valid && dt_module_ranges_build(dtp) != 0) {
		/* An optional index must not make a lookup fail under memory pressure. */
		for (dmp = dt_list_next(&dtp->dt_modlist); dmp;
		     dmp = dt_list_next(dmp)) {
			if (bsearch(&addr, dmp->dm_text_addrs, dmp->dm_text_addrs_size,
				    sizeof(dtrace_addr_range_t), dtrace_addr_range_cmp) ||
			    bsearch(&addr, dmp->dm_data_addrs, dmp->dm_data_addrs_size,
				    sizeof(dtrace_addr_range_t), dtrace_addr_range_cmp))
				return dmp;
		}
		return NULL;
	}
	index = dtp->dt_modranges;
	hi = dtp->dt_modranges_count;
	while (lo < hi) {
		size_t mid = lo + (hi - lo) / 2;

		if (index[mid].start <= addr)
			lo = mid + 1;
		else
			hi = mid;
	}
	/* Prefix maxima bound the backwards search, including overlapping ranges. */
	while (lo && index[lo - 1].max_last >= addr) {
		dt_module_range_t *r = &index[--lo];

		if (r->last >= addr && r->order < best) {
			dmp = r->module;
			best = r->order;
			if (best == 0)
				break;
		}
	}
	return dmp;
}

/*
 * Exported interface to look up a symbol by address.  We return the (possibly
 * partial) GElf_Sym and complete symbol information for the matching symbol.
 *
 * Only the st_info, st_value, and st_size fields of the GElf_Sym are guaranteed
 * to be populated: the st_shndx is populated but its only meaningful value is
 * SHN_UNDEF versus !SHN_UNDEF.
 *
 */
int
dtrace_lookup_by_addr(dtrace_hdl_t *dtp, GElf_Addr addr,
    GElf_Sym *symp, dtrace_syminfo_t *sip)
{
	dt_module_t *dmp;
	uint_t id;
	const dtrace_vector_t *v = dtp->dt_vector;

	if (v != NULL)
		return v->dtv_lookup_by_addr(dtp->dt_varg, addr, symp, sip);

	dmp = dt_module_by_addr(dtp, addr);

	if (dmp == NULL) {
		dt_dprintf("No module corresponds to %lx\n", addr);
		return dt_set_errno(dtp, EDT_NOSYMADDR);
	}

	if (dt_module_load(dtp, dmp) == -1)
		return -1; /* dt_errno is set for us */

	if ((dmp->dm_flags & DT_DM_KERNEL) &&
	    !(dmp->dm_flags & DT_DM_KERN_UNLOADED)) {
		dt_symbol_t *dt_symp;

		/* Loaded kernel modules use runtime addresses from /proc. */
		if (!dmp->dm_kernsyms)
			return dt_set_errno(dtp, EDT_NOSYMADDR);

		dt_symp = dt_symbol_by_addr(dmp->dm_kernsyms, addr);

		if (!dt_symp)
			return dt_set_errno(dtp, EDT_NOSYMADDR);

		if (sip != NULL) {
		    sip->object = dmp->dm_name;
		    sip->name = dt_symbol_name(dt_symp);
		    sip->id = 0;	/* undefined */
		}

		if (symp != NULL)
		    dt_symbol_to_elfsym(dtp, dt_symp, symp);

		return 0;
	} else {
		if (symp != NULL) {
			if (dt_module_symaddr(dmp, addr, symp, &id) == NULL)
				return dt_set_errno(dtp, EDT_NOSYMADDR);
		}

		if (sip != NULL) {
			sip->object = dmp->dm_name;

			if (symp != NULL) {
				sip->name = dt_module_symstr(dmp, id, symp);
				sip->id = id;
			} else {
				sip->name = NULL;
				sip->id = 0;
			}
		}
	}

	return 0;
}

int
dtrace_lookup_by_type(dtrace_hdl_t *dtp, const char *object, const char *name,
    dtrace_typeinfo_t *tip)
{
	dtrace_typeinfo_t ti;
	dt_module_t *dmp;
	int found = 0;
	ctf_id_t id;
	uint_t n;
	int justone;

	uint_t mask = 0; /* mask of dt_module flags to match */
	uint_t bits = 0; /* flag bits that must be present */

	if (object != DTRACE_OBJ_EVERY &&
	    object != DTRACE_OBJ_KMODS &&
	    object != DTRACE_OBJ_UMODS) {
		if ((dmp = dt_module_from_object(dtp, object)) == NULL)
			return -1; /* dt_errno is set for us */

		if (dt_module_load(dtp, dmp) == -1)
			return -1; /* dt_errno is set for us */
		n = 1;
		justone = 1;

	} else {
		if (object == DTRACE_OBJ_KMODS)
			mask = bits = DT_DM_KERNEL;
		else if (object == DTRACE_OBJ_UMODS)
			mask = DT_DM_KERNEL;

		dmp = dt_list_next(&dtp->dt_modlist);
		n = dt_htab_entries(dtp->dt_mods);
		justone = 0;
	}

	if (tip == NULL)
		tip = &ti;

	for (; n > 0; n--, dmp = dt_list_next(dmp)) {
		if ((dmp->dm_flags & mask) != bits)
			continue; /* failed to match required attributes */

		/*
		 * If we can't load the CTF container, continue on to the next
		 * module.  If our search was scoped to only one module then
		 * return immediately leaving dt_errno unmodified.
		 */
		if (dt_module_getctf(dtp, dmp) == NULL) {
			if (justone)
				return -1;
			continue;
		}

		/*
		 * Look up the type in the module's CTF container.  If our
		 * match is a forward declaration tag, save this choice in
		 * 'tip' and keep going in the hope that we will locate the
		 * underlying structure definition.  Otherwise just return.
		 */
		if ((id = dt_ctf_lookup_by_name(dtp, dmp->dm_ctfp,
					      name)) != CTF_ERR) {
			tip->dtt_object = dmp->dm_name;
			tip->dtt_ctfp = dmp->dm_ctfp;
			tip->dtt_type = id;

			if (ctf_type_kind(dmp->dm_ctfp, ctf_type_resolve(
			    dmp->dm_ctfp, id)) != CTF_K_FORWARD)
				return 0;

			found++;
		}
	}

	if (found == 0)
		return dt_set_errno(dtp, EDT_NOTYPE);

	return 0;
}

int
dtrace_symbol_type(dtrace_hdl_t *dtp, const GElf_Sym *symp,
    const dtrace_syminfo_t *sip, dtrace_typeinfo_t *tip)
{
	dt_module_t *dmp;
	int undefined = 0;

	tip->dtt_object = NULL;
	tip->dtt_ctfp = NULL;
	tip->dtt_type = CTF_ERR;

	if ((dmp = dt_module_lookup_by_name(dtp, sip->object)) == NULL)
		return dt_set_errno(dtp, EDT_NOMOD);

	if (dmp->dm_flags & DT_DM_KERNEL) {
		if (dmp->dm_flags & DT_DM_KERN_UNLOADED)
			return dt_set_errno(dtp, EDT_NOSYMADDR);

		if (dt_module_getctf(dtp, dmp) == NULL)
			return -1; /* errno is set for us */

		tip->dtt_ctfp = dmp->dm_ctfp;
		tip->dtt_type = ctf_lookup_variable(dmp->dm_ctfp, sip->name);

		if (tip->dtt_type == CTF_ERR) {
			dtp->dt_ctferr = ctf_errno(tip->dtt_ctfp);

			if (ctf_errno(tip->dtt_ctfp) == ECTF_NOTYPEDAT)
				undefined = 1;
			else
				return dt_set_errno(dtp, EDT_CTF);
		}

	} else if (symp->st_shndx == SHN_UNDEF) {
		undefined = 1;

	} else if (GELF_ST_TYPE(symp->st_info) != STT_FUNC) {
		if (dt_module_getctf(dtp, dmp) == NULL)
			return -1; /* errno is set for us */

		tip->dtt_ctfp = dmp->dm_ctfp;
		tip->dtt_type = ctf_lookup_by_symbol(dmp->dm_ctfp, sip->id);

		if (tip->dtt_type == CTF_ERR) {
			dtp->dt_ctferr = ctf_errno(tip->dtt_ctfp);
			return dt_set_errno(dtp, EDT_CTF);
		}

	} else {
		tip->dtt_ctfp = DT_FPTR_CTFP(dtp);
		tip->dtt_type = DT_FPTR_TYPE(dtp);
	}

	if (!undefined)
		goto out;

	/*
	 * Try externs for the module (if any).  If that fails, and we are
	 * looking for a symbol in a kernel module, default to uintptr_t.
	 */
	if (dmp->dm_extern != NULL) {
		dt_ident_t *idp	= dt_idhash_lookup(dmp->dm_extern, sip->name);

		if (idp == NULL)
			return dt_set_errno(dtp, EDT_NOSYM);

		tip->dtt_ctfp = idp->di_ctfp;
		tip->dtt_type = idp->di_type;
	} else if (dmp->dm_flags & DT_DM_KERNEL) {
		tip->dtt_ctfp = dmp->dm_ctfp;
		tip->dtt_type = dt_ctf_lookup_by_name(dtp, dmp->dm_ctfp,
						   "uintptr_t");

		if (tip->dtt_type == CTF_ERR)
			return dt_set_errno(dtp, EDT_NOSYM);
	} else
		return dt_set_errno(dtp, EDT_NOSYM);

out:
	tip->dtt_object = dmp->dm_name;
	return 0;
}

int
dt_module_sym_iter(const dt_module_t *dmp, proc_sym_f *func, void *arg)
{
	GElf_Sym	sym;
	uint_t		i;
	int		rv = 0;

	for (int t = 0; t < 2; t++) {
		const dt_module_symbols_t *syms = &dmp->dm_symbols[t];

		for (i = 0; i < syms->dm_aslen; i++) {
			const char *name;

			if (dmp->dm_ops == &dt_modops_64)
				dt_module_symgelf64(
				    ((const Elf64_Sym **)syms->dm_asmap)[i], &sym);
			else
				dt_module_symgelf32(
				    ((const Elf32_Sym **)syms->dm_asmap)[i], &sym);

			name = (const char *)syms->dm_strtab.cts_data + sym.st_name;

			/* Do not report symbols already covered by .symtab. */
			if (t != 0) {
				GElf_Sym primary;

				if (dmp->dm_ops->do_symname(
				    &dmp->dm_symbols[0],
				    name, &primary, NULL) != NULL)
					continue;
			}
			if ((rv = func(arg, &sym, name)) != 0)
				return rv;
		}
	}

	return rv;
}

static dtrace_objinfo_t *
dt_module_info(const dt_module_t *dmp, dtrace_objinfo_t *dto)
{
	dto->dto_name = dmp->dm_name;
	dto->dto_file = dmp->dm_file;
	dto->dto_flags = 0;

	if (dmp->dm_flags & DT_DM_KERNEL)
		dto->dto_flags |= DTRACE_OBJ_F_KERNEL;

	dto->dto_text_addrs = dmp->dm_text_addrs;
	dto->dto_text_addrs_size = dmp->dm_text_addrs_size;
	dto->dto_data_addrs = dmp->dm_data_addrs;
	dto->dto_data_addrs_size = dmp->dm_data_addrs_size;

	return dto;
}

int
dtrace_object_iter(dtrace_hdl_t *dtp, dtrace_obj_f *func, void *data)
{
	const dt_module_t *dmp = dt_list_next(&dtp->dt_modlist);
	dtrace_objinfo_t dto;
	int rv;

	for (; dmp != NULL; dmp = dt_list_next(dmp)) {
		if ((rv = (*func)(dtp, dt_module_info(dmp, &dto), data)) != 0)
			return rv;
	}

	return 0;
}

int
dtrace_object_info(dtrace_hdl_t *dtp, const char *object, dtrace_objinfo_t *dto)
{
	dt_module_t *dmp;

	if (object == DTRACE_OBJ_EVERY || object == DTRACE_OBJ_KMODS ||
	    object == DTRACE_OBJ_UMODS || dto == NULL)
		return dt_set_errno(dtp, EINVAL);

	if ((dmp = dt_module_from_object(dtp, object)) == NULL)
		return -1; /* dt_errno is set for us */

	if (dt_module_load(dtp, dmp) == -1)
		return -1; /* dt_errno is set for us */

	dt_module_info(dmp, dto);
	return 0;
}
