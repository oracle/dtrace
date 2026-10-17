/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <assert.h>
#include <byteswap.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <sys/mman.h>
#include <linux/btf.h>

#include <dt_impl.h>
#include <dt_bpf.h>
#include <dt_module.h>

typedef struct btf_header	btf_header_t;
typedef struct btf_type		btf_type_t;
typedef struct btf_enum		btf_enum_t;
typedef struct btf_array	btf_array_t;
typedef struct btf_member	btf_member_t;
typedef struct btf_param	btf_param_t;
typedef struct btf_var		btf_var_t;
typedef struct btf_var_secinfo	btf_var_secinfo_t;
typedef struct btf_decl_tag	btf_decl_tag_t;
typedef struct btf_enum64	btf_enum64_t;

struct dt_btf {
	void		*data;			/* raw BTF data */
	size_t		size;			/* raw BTF data size */
	btf_header_t	*hdr;			/* BTF header */
	char		*sdata;			/* string data */
	const dt_btf_t	*parent;		/* parent BTF */
	uint32_t	type_cnt;		/* number of types */
	btf_type_t	**types;		/* type table */
	ctf_id_t	*ctfids;		/* matching CTF type ids */
	uint32_t	*names;			/* name hash bucket heads */
	uint32_t	*name_next;		/* name hash chains */
	uint8_t		*ctf_state;		/* conversion state */
	uint32_t	*ctf_pending;		/* ids visited since snapshot */
	uint32_t	ctf_pending_cnt;
};

enum {
	DT_BTF_CTF_IDLE,
	DT_BTF_CTF_BUSY,
	DT_BTF_CTF_SHELL,
	DT_BTF_CTF_PREPARED
};

static btf_type_t	btf_void;

static const char *
dt_btf_errmsg(int err)
{
	return strerror(err);
}

static int
dt_btf_set_errno(dtrace_hdl_t *dtp, int err)
{
	dtp->dt_btferr = err;
	return -1;
}

static void *
dt_btf_set_load_errno(dtrace_hdl_t *dtp, int err)
{
	dtp->dt_btferr = err;
	return NULL;
}

static void
dt_btf_error(dtrace_hdl_t *dtp, int eid, const char *format, ...)
{
	va_list	ap;

	va_start(ap, format);
	dt_set_errmsg(dtp, dt_errtag(eid), NULL, NULL, 0, format, ap);
	dt_set_errno(dtp, EDT_BPF);
	va_end(ap);
}

static btf_type_t *
dt_btf_type_by_id(const dt_btf_t *btf, int32_t id)
{
	const dt_btf_t	*shared_btf;
	uint32_t	shared_cnt;

	if (id == 0)
		return &btf_void;

	shared_btf = btf->parent;
	if (shared_btf != NULL) {
		shared_cnt = shared_btf->type_cnt;
		if (id < shared_cnt)
			return shared_btf->types[id];

		if (shared_cnt > 0)
			id -= shared_cnt - 1;
	}

	if (id < btf->type_cnt && id > 0)
		return btf->types[id];

	return NULL;
}

static btf_type_t *
dt_btf_real_type_by_id(const dt_btf_t *btf, int32_t id)
{
	btf_type_t	*type = dt_btf_type_by_id(btf, id);
	uint64_t	remaining = 1 + (uint64_t)btf->type_cnt;

	if (btf->parent != NULL)
		remaining += btf->parent->type_cnt;

	/* A longer chain must revisit a type, so reject cyclic BTF references. */
	while (type != NULL && remaining > 0) {
		remaining--;
		switch (BTF_INFO_KIND(type->info)) {
		case BTF_KIND_CONST:
		case BTF_KIND_FUNC:
		case BTF_KIND_RESTRICT:
		case BTF_KIND_TYPE_TAG:
		case BTF_KIND_TYPEDEF:
		case BTF_KIND_VOLATILE:
			type = dt_btf_type_by_id(btf, type->type);
			continue;
		default:
			return type;
		}
	}

	return NULL;
}

static const char *
dt_btf_get_string(const dt_btf_t *btf, uint32_t off)
{
	const dt_btf_t	*shared_btf;
	int32_t		shared_len;

	shared_btf = btf->parent;
	if (shared_btf != NULL && shared_btf->hdr != NULL) {
		shared_len = shared_btf->hdr->str_len;
		if (off < shared_len)
			return shared_btf->sdata + off;

		off -= shared_len;
	}

	if (off < btf->hdr->str_len)
		return btf->sdata + off;

	return NULL;
}

/* Index names by BTF kind, preserving the first matching BTF id. */
static int
dt_btf_index_names(dtrace_hdl_t *dtp, dt_btf_t *btf)
{
	uint32_t i;

	btf->names = dt_calloc(dtp, btf->type_cnt, sizeof(uint32_t));
	btf->name_next = dt_calloc(dtp, btf->type_cnt, sizeof(uint32_t));
	if (btf->names == NULL || btf->name_next == NULL)
		return dt_btf_set_errno(dtp, ENOMEM);

	for (i = btf->type_cnt - 1; i > 0; i--) {
		btf_type_t *type = btf->types[i];
		const char *name = dt_btf_get_string(btf, type->name_off);
		uint32_t slot;

		if (name[0] == '\0')
			continue;
		slot = str2hval(name, BTF_INFO_KIND(type->info)) %
		       btf->type_cnt;
		btf->name_next[i] = btf->names[slot];
		btf->names[slot] = i;
	}

	return 0;
}

static int
dt_btf_validate_header(dtrace_hdl_t *dtp, dt_btf_t *btf)
{
	btf_header_t	*hdr = (btf_header_t *)btf->data;

	if (btf->size < sizeof(btf_header_t)) {
		dt_dprintf("BTF truncated header (%ld)\n", btf->size);
		return dt_btf_set_errno(dtp, EINVAL);
	}

	hdr = (btf_header_t *)btf->data;

	/* Validate magic number. */
	if (hdr->magic != BTF_MAGIC) {
		dt_dprintf("BTF magic mismatch (%04x, expected %04x)\n",
			   hdr->magic, BTF_MAGIC);
		return dt_btf_set_errno(dtp, EINVAL);
	}

	/* Validate header length. */
	if (hdr->hdr_len < sizeof(btf_header_t) ||
	    hdr->hdr_len >= btf->size) {
		dt_dprintf("BTF bad header length (%d)\n", hdr->hdr_len);
		return dt_btf_set_errno(dtp, EINVAL);
	}

	/* Validate data offsets and lengths. */
	if (hdr->type_off % 4) {
		dt_dprintf("BTF type offset misaligned (%d)\n", hdr->type_off);
		return dt_btf_set_errno(dtp, EINVAL);
	}
	if (hdr->type_off >= btf->size - hdr->hdr_len) {
		dt_dprintf("BTF bad type offset (%d)\n", hdr->type_off);
		return dt_btf_set_errno(dtp, EINVAL);
	}
	if (hdr->str_off <= hdr->type_off ||
	    hdr->str_off >= btf->size - hdr->hdr_len) {
		dt_dprintf("BTF bad string offset (%d)\n", hdr->str_off);
		return dt_btf_set_errno(dtp, EINVAL);
	}
	if (hdr->type_len > hdr->str_off - hdr->type_off) {
		dt_dprintf("BTF bad type section length (%d)\n", hdr->type_len);
		return dt_btf_set_errno(dtp, EINVAL);
	}
	if (hdr->str_len > btf->size - hdr->hdr_len - hdr->str_off) {
		dt_dprintf("BTF bad string section length (%d)\n",
			   hdr->str_len);
		return dt_btf_set_errno(dtp, EINVAL);
	}
	if (((char *)btf->data)[hdr->hdr_len + hdr->str_off + hdr->str_len - 1] != 0) {
		dt_dprintf("BTF string section is not NUL-terminated\n");
		return dt_btf_set_errno(dtp, EINVAL);
	}

	btf->hdr = hdr;

	return 0;
}

static ssize_t
dt_btf_type_size(const char *ptr)
{
	btf_type_t	*type = (btf_type_t *)ptr;
	ssize_t		size = sizeof(btf_type_t);
	int		vlen = BTF_INFO_VLEN(type->info);

	switch (BTF_INFO_KIND(type->info)) {
	case BTF_KIND_INT:
		return size + sizeof(uint32_t);
	case BTF_KIND_ARRAY:
		return size + sizeof(btf_array_t);
	case BTF_KIND_STRUCT:
	case BTF_KIND_UNION:
		return size + vlen * sizeof(btf_member_t);
	case BTF_KIND_ENUM:
		return size + vlen * sizeof(btf_enum_t);
	case BTF_KIND_FUNC_PROTO:
		return size + vlen * sizeof(btf_param_t);
	case BTF_KIND_VAR:
		return size + sizeof(btf_var_t);
	case BTF_KIND_DATASEC:
		return size + vlen * sizeof(btf_var_secinfo_t);
	case BTF_KIND_DECL_TAG:
		return size + sizeof(btf_decl_tag_t);
	case BTF_KIND_TYPE_TAG:
		return size;
	case BTF_KIND_ENUM64:
		return size + vlen * sizeof(btf_enum64_t);
	case BTF_KIND_PTR:
	case BTF_KIND_FWD:
	case BTF_KIND_TYPEDEF:
	case BTF_KIND_VOLATILE:
	case BTF_KIND_CONST:
	case BTF_KIND_RESTRICT:
	case BTF_KIND_FUNC:
	case BTF_KIND_FLOAT:
		return size;
	default:
		return -EINVAL;
	}

	return -EINVAL;
}

static int
dt_btf_decode(dtrace_hdl_t *dtp, dt_btf_t *btf)
{
	btf_header_t	*hdr;
	char		*ptr, *end;
	int32_t		idx;
	char		*tdata;

	if (dt_btf_validate_header(dtp, btf) == -1)
		return dt_btf_set_errno(dtp, EINVAL);

	hdr = btf->hdr;
	btf->sdata = (char *)btf->data + hdr->hdr_len + hdr->str_off;
	tdata = (char *)btf->data + hdr->hdr_len + hdr->type_off;

	/* First count how many types there really are. */
	ptr = tdata;
	end = tdata + hdr->type_len;
	idx = 1;
	while (ptr + sizeof(btf_type_t) <= end) {
		ssize_t		size = dt_btf_type_size(ptr);
		btf_type_t	*type = (btf_type_t *)ptr;

		if (size < 0 || ptr + size > end ||
		    dt_btf_get_string(btf, type->name_off) == NULL)
			return dt_btf_set_errno(dtp, EINVAL);

		idx++;
		ptr += size;
	}
	if (ptr != end)
		return dt_btf_set_errno(dtp, EINVAL);

	/* Next, populate the type offsets table. */
	btf->type_cnt = idx;
	btf->types = dt_calloc(dtp, btf->type_cnt, sizeof(btf_type_t *));
	if (btf->types == NULL)
		return dt_btf_set_errno(dtp, ENOMEM);

	ptr = tdata;
	idx = 1;
	while (ptr + sizeof(btf_type_t) <= end) {
		ssize_t	size = dt_btf_type_size(ptr);

		if (size < 0)
			return dt_btf_set_errno(dtp, EINVAL);

		btf->types[idx++] = (btf_type_t *)ptr;
		ptr += size;
	}
	if (ptr != end)
		return dt_btf_set_errno(dtp, EINVAL);

	return dt_btf_index_names(dtp, btf);
}

/*
 * Try to load BTF data from the given file.  We use FILE-based IO primitives
 * instead of fd-based IO primitives because the BTF data is usually provided
 * by sysfs pseudo-files and the fd-based IO primitives do not seem to work
 * with that.
 */
static dt_btf_t *
dt_btf_load(dtrace_hdl_t *dtp, const char *fn)
{
	FILE		*fp = NULL;
	dt_btf_t	*btf = NULL;
	btf_header_t	hdr;
	Elf		*elf = NULL;
	size_t		shstrs;
	Elf_Scn		*sp = NULL;
	int		err = 0;

	btf = dt_zalloc(dtp, sizeof(dt_btf_t));
	if (btf == NULL)
		return dt_btf_set_load_errno(dtp, ENOMEM);

	fp = fopen(fn, "rb");
	if (fp == NULL) {
		dtp->dt_btferr = errno;
		goto fail;
	}

	/* First see whether this might be a file with raw BTF data. */
	if (fread(&hdr, 1, sizeof(hdr), fp) < sizeof(hdr)) {
		dtp->dt_btferr = ferror(fp) ? EIO : EINVAL;
		goto fail;
	}

	rewind(fp);
	if (hdr.magic == BTF_MAGIC) {
		struct stat	st;

		if (fstat(fileno(fp), &st) == -1) {
			dtp->dt_btferr = errno;
			goto fail;
		}

		btf->data = dt_alloc(dtp, st.st_size);
		if (btf->data == NULL) {
			dtp->dt_btferr = ENOMEM;
			goto fail;
		}

		if (fread(btf->data, 1, st.st_size, fp) < st.st_size) {
			dtp->dt_btferr = ferror(fp) ? EIO : EINVAL;
			goto fail;
		}

		btf->size = st.st_size;
	} else {
		/* Next see whether this might be an ELF file with BTF data. */
		elf = elf_begin(fileno(fp), ELF_C_READ_MMAP, NULL);
		if (elf == NULL) {
			dtp->dt_btferr = EINVAL;
			goto elf_fail;
		}
		if (elf_getshdrstrndx(elf, &shstrs) == -1)
			goto elf_fail;

		while ((sp = elf_nextscn(elf, sp)) != NULL) {
			GElf_Shdr	sh;
			Elf_Data	*edata;
			const char	*name;

			/* Skip malformed sections. */
			if (gelf_getshdr(sp, &sh) == NULL ||
			    sh.sh_type == SHT_NULL)
				continue;

			name = elf_strptr(elf, shstrs, sh.sh_name);
			if (name == NULL || strcmp(name, ".BTF") != 0)
				continue;

			/* Found the section we need. */
			edata = elf_getdata(sp, 0);
			if (edata == NULL)
				goto elf_fail;

			btf->data = dt_alloc(dtp, edata->d_size);
			if (btf->data == NULL) {
				dtp->dt_btferr = ENOMEM;
				goto fail;
			}

			memcpy(btf->data, edata->d_buf, edata->d_size);

			btf->size = edata->d_size;
			break;
		}
		if (btf->data == NULL) {
			dtp->dt_btferr = ENOENT;
			goto fail;
		}
	}

	if (elf != NULL)
		elf_end(elf);
	fclose(fp);
	return btf;

elf_fail:
	dtp->dt_btferr = EINVAL;
	err = elf_errno();
	dt_btf_error(dtp, 0, "ELF: %s", elf_errmsg(err));

fail:
	if (elf != NULL)
		elf_end(elf);
	if (fp != NULL)
		fclose(fp);
	dt_dprintf("Cannot open BTF file %s: %s\n", fn,
			   dt_btf_errmsg(dtp->dt_btferr));
	dt_btf_destroy(dtp, btf);
	return NULL;
}

dt_btf_t *
dt_btf_load_file(dtrace_hdl_t *dtp, const char *fn, const dt_btf_t *parent)
{
	dt_btf_t	*btf;

	/*
	 * Missing BTF or allocation failure returns NULL; invalid data leaves
	 * an empty BTF.
	 */
	btf = dt_btf_load(dtp, fn);
	if (btf == NULL)
		return NULL;

	btf->parent = parent;

	/* Decode the loaded BTF data. */
	if (btf->data && dt_btf_decode(dtp, btf) == -1) {
		if (dtp->dt_btferr == ENOMEM) {
			dt_btf_destroy(dtp, btf);
			return NULL;
		}

		/*
		 * Failure to decode the data leaves us with just one option:
		 * pretend the module has no type data beyond its parent's
		 * data.
		 */
		dt_dprintf("Cannot decode BTF data %s: %s\n", fn,
			   dt_btf_errmsg(dtp->dt_btferr));

		dt_free(dtp, btf->data);
		dt_free(dtp, btf->types);
		dt_free(dtp, btf->names);
		dt_free(dtp, btf->name_next);
		memset(btf, 0, sizeof(*btf));
		btf->parent = parent;
	}

	dt_dprintf("BTF file %s: %d types\n", fn, btf->type_cnt);

	return btf;
}

dt_btf_t *
dt_btf_load_module(dtrace_hdl_t *dtp, dt_module_t *dmp)
{
	char		*fn = NULL;
	int		rc = 0;
	const dt_btf_t	*parent = NULL;

	assert(dmp != NULL);

	/* If already tried this for the module, return what we have. */
	if (dmp->dm_flags & DT_DM_BTF_LOADED)
		return dmp->dm_btf;

	/*
	 * If the module is not vmlinux, we will use the vmlinux BTF data as the
	 * parent.
	 */
	if (dmp != dtp->dt_exec) {
		parent = dtp->dt_exec->dm_btf;
		/* If we do not have it yet, try to load it. */
		if (parent == NULL)
			parent = dt_btf_load_module(dtp, dtp->dt_exec);
		/* If loading failed, the module cannot have BTF data either. */
		if (parent == NULL)
			return NULL;
	}

	/*
	 * Default: /sys/kernel/btf/<module>
	 * If "none", disable BTF.
	 * Otherwise: <BTF path>/<module>
	 */
	if (dtp->dt_btf_path == NULL)
		rc = asprintf(&fn, "/sys/kernel/btf/%s", dmp->dm_name);
	else if (strcmp(dtp->dt_btf_path, "none") == 0)
		return NULL;
	else
		rc = asprintf(&fn, "%s/%s", dtp->dt_btf_path, dmp->dm_name);

	if (rc == -1)
		return dt_btf_set_load_errno(dtp, ENOMEM);

	dmp->dm_btf = dt_btf_load_file(dtp, fn, parent);
	free(fn);

	if (dmp->dm_btf == NULL) {
		if (dtp->dt_btferr != ENOENT)
			return NULL;

		/* A missing module BTF file still inherits its parent's types. */
		if (parent != NULL) {
			dmp->dm_btf = dt_zalloc(dtp, sizeof(dt_btf_t));
			if (dmp->dm_btf == NULL)
				return dt_btf_set_load_errno(dtp, ENOMEM);

			dmp->dm_btf->parent = parent;
		}
	}

	dmp->dm_flags |= DT_DM_BTF_LOADED;

	return dmp->dm_btf;
}

void
dt_btf_destroy(dtrace_hdl_t *dtp, dt_btf_t *btf)
{
	if (!btf)
		return;

	dt_free(dtp, btf->data);
	dt_free(dtp, btf->types);
	dt_free(dtp, btf->ctfids);
	dt_free(dtp, btf->names);
	dt_free(dtp, btf->name_next);
	dt_free(dtp, btf->ctf_state);
	dt_free(dtp, btf->ctf_pending);
	dt_free(dtp, btf);
}

#ifdef HAVE_LIBCTF
static ctf_id_t
dt_ctf_set_errno(dtrace_hdl_t *dtp, int err)
{
	dtp->dt_ctferr = err;
	dt_set_errno(dtp, EDT_CTF);
	return CTF_ERR;
}

static ctf_id_t
dt_ctf_error(dtrace_hdl_t *dtp, ctf_dict_t *ctf)
{
	return dt_ctf_set_errno(dtp, ctf_errno(ctf));
}

static uint32_t dt_btf_lookup_name_kind2(const dt_btf_t *, int32_t,
					 const char *, uint32_t);

static ctf_id_t dt_btf_add_to_ctf(dtrace_hdl_t *, dt_btf_t *,
				ctf_dict_t *, uint32_t);

static ctf_id_t
dt_btf_add_type(dtrace_hdl_t *dtp, dt_btf_t *btf, ctf_dict_t *ctf,
		uint32_t type_id)
{
	btf_type_t	*type;
	int		kind, vlen;
	const char	*name;
	ctf_id_t	ctfid;
	ctf_encoding_t	enc;

	type = btf->types[type_id];
	kind = BTF_INFO_KIND(type->info);
	vlen = BTF_INFO_VLEN(type->info);
	name = dt_btf_get_string(btf, type->name_off);
	if (name == NULL)
		return dt_ctf_set_errno(dtp, ECTF_NAMELEN);

	if (name[0] && btf->ctfids[type_id] == CTF_ERR) {
		char	n[DT_TYPE_NAMELEN];

		/* Do we already have this type? */
		switch (kind) {
		case BTF_KIND_ENUM:
		case BTF_KIND_ENUM64:
			if (snprintf(n, sizeof(n), "enum %s", name) < 0)
				return dt_ctf_set_errno(dtp, ECTF_NAMELEN);

			ctfid = ctf_lookup_by_name(ctf, n);
			break;
		case BTF_KIND_UNION:
			if (snprintf(n, sizeof(n), "union %s", name) < 0)
				return dt_ctf_set_errno(dtp, ECTF_NAMELEN);
	
			ctfid = ctf_lookup_by_name(ctf, n);
			break;
		case BTF_KIND_STRUCT:
			if (snprintf(n, sizeof(n), "struct %s", name) < 0)
				return dt_ctf_set_errno(dtp, ECTF_NAMELEN);
	
			ctfid = ctf_lookup_by_name(ctf, n);
			break;
		default:
			ctfid = ctf_lookup_by_name(ctf, name);
		}

		if (ctfid != CTF_ERR &&
		    !((kind == BTF_KIND_STRUCT || kind == BTF_KIND_UNION) &&
		      ctf_type_kind(ctf, ctfid) == CTF_K_FORWARD)) {
			btf->ctfids[type_id] = ctfid;
			return ctfid;
		}
	}

	switch (kind) {
	case BTF_KIND_INT: {
		uint32_t	data = *(uint32_t *)(type + 1);
		uint32_t	benc = BTF_INT_ENCODING(data);

		enc.cte_format = 0;
		enc.cte_offset = BTF_INT_OFFSET(data);
		enc.cte_bits = BTF_INT_BITS(data);

		if (benc & BTF_INT_SIGNED)
			enc.cte_format |= CTF_INT_SIGNED;
		if (benc & BTF_INT_CHAR)
			enc.cte_format |= CTF_INT_CHAR;
		if (benc & BTF_INT_BOOL)
			enc.cte_format |= CTF_INT_BOOL;

		/* Some special case handling. */
		if (enc.cte_bits == 8) {
			if (strcmp(name, "_Bool") == 0)
				enc.cte_format = CTF_INT_BOOL;
			else
				enc.cte_format = CTF_INT_CHAR;
		}

		ctfid = ctf_add_integer(ctf, CTF_ADD_ROOT, name, &enc);
		btf->ctfids[type_id] = ctfid;

		return ctfid == CTF_ERR ? dt_ctf_error(dtp, ctf) : ctfid;
	}
	case BTF_KIND_FLOAT: {
		switch (type->size) {
		case 4:
			enc.cte_format = CTF_FP_SINGLE;
			break;
		case 8:
			enc.cte_format = CTF_FP_DOUBLE;
			break;
		case 16:
			enc.cte_format = CTF_FP_LDOUBLE;
			break;
		}

		enc.cte_offset = 0;
		enc.cte_bits = type->size * 8;

		ctfid = ctf_add_float(ctf, CTF_ADD_ROOT, name, &enc);
		btf->ctfids[type_id] = ctfid;

		return ctfid == CTF_ERR ? dt_ctf_error(dtp, ctf) : ctfid;
	}
	case BTF_KIND_ENUM: {
		int		i;
		btf_enum_t	*item = (btf_enum_t *)(type + 1);

		ctfid = ctf_add_enum(ctf, CTF_ADD_ROOT, name);
		if (ctfid == CTF_ERR)
			return dt_ctf_error(dtp, ctf);

		for (i = 0; i < vlen; i++, item++) {
			int	err;
			const char	*iname;

			iname = dt_btf_get_string(btf, item->name_off);
			if (iname == NULL)
				return dt_ctf_set_errno(dtp, ECTF_NAMELEN);
			err = ctf_add_enumerator(ctf, ctfid, iname, item->val);
			if (err == CTF_ERR)
				return dt_ctf_error(dtp, ctf);
		}

		btf->ctfids[type_id] = ctfid;

		return ctfid == CTF_ERR ? dt_ctf_error(dtp, ctf) : ctfid;
	}
	case BTF_KIND_PTR: {
		ctfid = dt_btf_add_to_ctf(dtp, btf, ctf, type->type);
		if (ctfid == CTF_ERR)
			return CTF_ERR;		/* errno already set */

		ctfid = ctf_add_pointer(ctf, CTF_ADD_ROOT, ctfid);
		btf->ctfids[type_id] = ctfid;

		return ctfid == CTF_ERR ? dt_ctf_error(dtp, ctf) : ctfid;
	}
	case BTF_KIND_CONST: {
		ctfid = dt_btf_add_to_ctf(dtp, btf, ctf, type->type);
		if (ctfid == CTF_ERR)
			return CTF_ERR;		/* errno already set */

		ctfid = ctf_add_const(ctf, CTF_ADD_ROOT, ctfid);
		btf->ctfids[type_id] = ctfid;

		return ctfid == CTF_ERR ? dt_ctf_error(dtp, ctf) : ctfid;
	}
	case BTF_KIND_VOLATILE: {
		ctfid = dt_btf_add_to_ctf(dtp, btf, ctf, type->type);
		if (ctfid == CTF_ERR)
			return CTF_ERR;		/* errno already set */

		ctfid = ctf_add_volatile(ctf, CTF_ADD_ROOT, ctfid);
		btf->ctfids[type_id] = ctfid;

		return ctfid == CTF_ERR ? dt_ctf_error(dtp, ctf) : ctfid;
	}
	case BTF_KIND_RESTRICT: {
		ctfid = dt_btf_add_to_ctf(dtp, btf, ctf, type->type);
		if (ctfid == CTF_ERR)
			return CTF_ERR;		/* errno already set */

		ctfid = ctf_add_restrict(ctf, CTF_ADD_ROOT, ctfid);
		btf->ctfids[type_id] = ctfid;

		return ctfid == CTF_ERR ? dt_ctf_error(dtp, ctf) : ctfid;
	}
	case BTF_KIND_ARRAY: {
		btf_array_t	*arr = (btf_array_t *)(type + 1);
		ctf_arinfo_t	arp;

		arp.ctr_contents = dt_btf_add_to_ctf(dtp, btf, ctf, arr->type);
		if (arp.ctr_contents == CTF_ERR)
			return CTF_ERR;
		arp.ctr_index = dt_btf_add_to_ctf(dtp, btf, ctf, arr->index_type);
		if (arp.ctr_contents == CTF_ERR)
			return CTF_ERR;
		arp.ctr_nelems = arr->nelems;

		ctfid = ctf_add_array(ctf, CTF_ADD_ROOT, &arp);
		btf->ctfids[type_id] = ctfid;

		return ctfid == CTF_ERR ? dt_ctf_error(dtp, ctf) : ctfid;
	}
	case BTF_KIND_TYPEDEF: {
		ctfid = dt_btf_add_to_ctf(dtp, btf, ctf, type->type);
		if (ctfid == CTF_ERR)
			return CTF_ERR;		/* errno already set */

		ctfid = ctf_add_typedef(ctf, CTF_ADD_ROOT, name, ctfid);
		btf->ctfids[type_id] = ctfid;

		return ctfid == CTF_ERR ? dt_ctf_error(dtp, ctf) : ctfid;
	}
	case BTF_KIND_STRUCT:
	case BTF_KIND_UNION: {
		int		i;
		btf_member_t	*item = (btf_member_t *)(type + 1);

		/* The preparation pass reserved this aggregate's type id. */
		ctfid = btf->ctfids[type_id];
		assert(ctfid != CTF_ERR);

		for (i = 0; i < vlen; i++, item++) {
			int		err;
			ctf_id_t	mbid;
			uint32_t	offset = item->offset;
			const char	*mname;

			mbid = dt_btf_add_to_ctf(dtp, btf, ctf, item->type);
			if (mbid == CTF_ERR)
				return CTF_ERR;
			mname = dt_btf_get_string(btf, item->name_off);
			if (mname == NULL)
				return dt_ctf_set_errno(dtp, ECTF_NAMELEN);

			if (BTF_INFO_KFLAG(type->info)) {
				enc.cte_format = 0;
				enc.cte_offset = 0;
				enc.cte_bits = BTF_MEMBER_BITFIELD_SIZE(item->offset);
				offset = BTF_MEMBER_BIT_OFFSET(item->offset);

				if (enc.cte_bits > 0) {
					int	kind;

					/* Resolve member type for CTF. */
					mbid = ctf_type_resolve(ctf, mbid);
					kind = ctf_type_kind(ctf, mbid);

					if (kind != CTF_K_INTEGER &&
					    kind != CTF_K_FLOAT &&
					    kind != CTF_K_ENUM)
						return dt_ctf_set_errno(
							dtp, ECTF_NOTINTFP);

					mbid = ctf_add_slice(ctf,
							     CTF_ADD_NONROOT,
							     mbid, &enc);
					if (mbid == CTF_ERR)
						return dt_ctf_error(dtp, ctf);

					/* Fall-through */
				}

				/* Fall-through - add as regular member. */
			}

			err = ctf_add_member_offset(ctf, ctfid, mname, mbid,
						    offset);
			if (err == CTF_ERR)
				return dt_ctf_error(dtp, ctf);
		}

		return ctfid;
	}
	case BTF_KIND_FUNC_PROTO: {
		int		i;
		ctf_id_t	argv[vlen];
		btf_param_t	*item = (btf_param_t *)(type + 1);
		ctf_funcinfo_t	ctc;

		ctc.ctc_flags = 0;
		ctc.ctc_argc = vlen;
		ctc.ctc_return = dt_btf_add_to_ctf(dtp, btf, ctf, type->type);
		if (ctc.ctc_return == CTF_ERR)
			return CTF_ERR;

		for (i = 0; i < vlen; i++, item++) {
			argv[i] = dt_btf_add_to_ctf(dtp, btf, ctf, item->type);
			if (argv[i] == CTF_ERR)
				return CTF_ERR;
		}

		ctfid = ctf_add_function(ctf, CTF_ADD_ROOT, &ctc, argv);
		btf->ctfids[type_id] = ctfid;

		return ctfid == CTF_ERR ? dt_ctf_error(dtp, ctf) : ctfid;
	}
	case BTF_KIND_FWD: {
		uint32_t	kind;

		kind = BTF_INFO_KFLAG(type->info) ? CTF_K_UNION : CTF_K_STRUCT;
		ctfid = ctf_add_forward(ctf, CTF_ADD_ROOT, name, kind);
		btf->ctfids[type_id] = ctfid;

		return ctfid == CTF_ERR ? dt_ctf_error(dtp, ctf) : ctfid;
	}
	case BTF_KIND_TYPE_TAG: {
		/*
		 * A type tag (e.g. __rcu, __user, __percpu) is a transparent
		 * annotation on the type it wraps.  CTF has no equivalent, so
		 * we resolve straight through to the underlying type, just as
		 * we do for const/volatile/restrict.  Treating it as ignored
		 * (mapping it to void) would leave every tagged member -- such
		 * as task_struct.real_parent (__rcu) -- resolving to "void",
		 * which breaks translator compilation on kernels whose BTF
		 * carries type tags.
		 */
		ctfid = dt_btf_add_to_ctf(dtp, btf, ctf, type->type);
		if (ctfid == CTF_ERR)
			return CTF_ERR;		/* errno already set */

		btf->ctfids[type_id] = ctfid;

		return ctfid;
	}
	case BTF_KIND_VAR:
	case BTF_KIND_DATASEC:
	case BTF_KIND_DECL_TAG:
	case BTF_KIND_ENUM64:
	case BTF_KIND_FUNC:
		return btf->ctfids[0];		/* Ignored for CTF */
	default:
		return dt_ctf_error(dtp, ctf);
	}

	assert(0);
}

/* Resolve shared ids in the shared dictionary, which also grows lazily. */
static ctf_id_t
dt_btf_add_to_ctf(dtrace_hdl_t *dtp, dt_btf_t *btf, ctf_dict_t *ctf,
		  uint32_t type_id)
{
	const dt_btf_t *parent = btf->parent;
	ctf_id_t id;

	if (parent != NULL && dt_module_shared_ctf(dtp) != NULL) {
		if (type_id < parent->type_cnt)
			return dt_btf_add_to_ctf(dtp, (dt_btf_t *)parent,
					       dt_module_shared_ctf(dtp), type_id);
		if (parent->type_cnt > 0)
			type_id -= parent->type_cnt - 1;
	}

	if (type_id >= btf->type_cnt && type_id != 0)
		return dt_ctf_set_errno(dtp, ECTF_BADID);
	if (btf->ctfids[type_id] != CTF_ERR)
		return btf->ctfids[type_id];
	if (btf->ctf_state[type_id] == DT_BTF_CTF_BUSY)
		return dt_ctf_set_errno(dtp, ECTF_CORRUPT);

	btf->ctf_state[type_id] = DT_BTF_CTF_BUSY;
	id = dt_btf_add_type(dtp, btf, ctf, type_id);
	btf->ctf_state[type_id] = DT_BTF_CTF_IDLE;
	if (id != CTF_ERR)
		btf->ctfids[type_id] = id;
	return id;
}

/*
 * Reserve aggregate shells before converting references.  Starting at a
 * typedef or pointer must handle the same recursive graphs as starting at
 * the struct itself.  The pending list also journals cache changes for
 * rollback.  SHELL denotes an aggregate whose members still need adding;
 * PREPARED denotes a visited non-aggregate in this preparation pass.
 */
static int
dt_btf_ctf_prepare(dtrace_hdl_t *dtp, dt_btf_t *btf, ctf_dict_t *ctf,
		   uint32_t type_id)
{
	const dt_btf_t *parent = btf->parent;
	btf_type_t *type;
	uint32_t kind, vlen, i;

	if (parent != NULL && dt_module_shared_ctf(dtp) != NULL) {
		if (type_id < parent->type_cnt)
			return dt_btf_ctf_prepare(dtp, (dt_btf_t *)parent,
						  dt_module_shared_ctf(dtp), type_id);
		if (parent->type_cnt > 0)
			type_id -= parent->type_cnt - 1;
	}
	if (type_id >= btf->type_cnt && type_id != 0)
		return dt_ctf_set_errno(dtp, ECTF_BADID);
	if (btf->ctfids[type_id] != CTF_ERR || btf->ctf_state[type_id])
		return 0;

	btf->ctf_pending[btf->ctf_pending_cnt++] = type_id;
	btf->ctf_state[type_id] = DT_BTF_CTF_PREPARED;
	type = btf->types[type_id];
	kind = BTF_INFO_KIND(type->info);
	vlen = BTF_INFO_VLEN(type->info);

	switch (kind) {
	case BTF_KIND_STRUCT:
	case BTF_KIND_UNION: {
		const char *name = dt_btf_get_string(btf, type->name_off);
		char n[DT_TYPE_NAMELEN];
		ctf_id_t id = CTF_ERR;
		btf_member_t *members = (btf_member_t *)(type + 1);

		if (name[0] != '\0') {
			snprintf(n, sizeof(n), "%s %s",
				 kind == BTF_KIND_STRUCT ? "struct" : "union",
				 name);
			id = ctf_lookup_by_name(ctf, n);
		}
		if (id != CTF_ERR && ctf_type_kind(ctf, id) != CTF_K_FORWARD) {
			btf->ctfids[type_id] = id;
			return 0;
		}
		if (kind == BTF_KIND_STRUCT)
			id = ctf_add_struct_sized(ctf, CTF_ADD_ROOT, name,
						  type->size);
		else
			id = ctf_add_union_sized(ctf, CTF_ADD_ROOT, name,
						 type->size);
		if (id == CTF_ERR)
			return dt_ctf_error(dtp, ctf);
		btf->ctfids[type_id] = id;
		btf->ctf_state[type_id] = DT_BTF_CTF_SHELL;
		for (i = 0; i < vlen; i++) {
			if (dt_btf_ctf_prepare(dtp, btf, ctf,
					       members[i].type) < 0)
				return -1;
		}
		return 0;
	}
	case BTF_KIND_FWD: {
		const char *name = dt_btf_get_string(btf, type->name_off);
		uint32_t target = dt_btf_lookup_name_kind2(btf, 0, name,
			BTF_INFO_KFLAG(type->info) ? BTF_KIND_UNION :
						   BTF_KIND_STRUCT);
		uint32_t base = parent != NULL && parent->type_cnt > 0 ?
				parent->type_cnt - 1 : 0;

		/* Prefer a definition before publishing a forward type. */
		if (target != BTF_TYPE_NONE) {
			if (dt_btf_ctf_prepare(dtp, btf, ctf,
					       target + base) < 0)
				return -1;
			btf->ctfids[type_id] = btf->ctfids[target];
		}
		return 0;
	}
	case BTF_KIND_ARRAY: {
		btf_array_t *array = (btf_array_t *)(type + 1);

		if (dt_btf_ctf_prepare(dtp, btf, ctf, array->index_type) < 0)
			return -1;
		return dt_btf_ctf_prepare(dtp, btf, ctf, array->type);
	}
	case BTF_KIND_FUNC_PROTO: {
		btf_param_t *params = (btf_param_t *)(type + 1);

		for (i = 0; i < vlen; i++) {
			if (dt_btf_ctf_prepare(dtp, btf, ctf,
					       params[i].type) < 0)
				return -1;
		}
		/* Fall through to prepare the return type. */
	}
	case BTF_KIND_PTR:
	case BTF_KIND_CONST:
	case BTF_KIND_VOLATILE:
	case BTF_KIND_RESTRICT:
	case BTF_KIND_TYPEDEF:
	case BTF_KIND_TYPE_TAG:
		return dt_btf_ctf_prepare(dtp, btf, ctf, type->type);
	default:
		return 0;
	}
}

static int
dt_btf_ctf_finish(dtrace_hdl_t *dtp, dt_btf_t *btf, ctf_dict_t *ctf)
{
	uint32_t i;

	for (i = 0; i < btf->ctf_pending_cnt; i++) {
		uint32_t id = btf->ctf_pending[i];

		if (btf->ctf_state[id] == DT_BTF_CTF_SHELL &&
		    dt_btf_add_type(dtp, btf, ctf, id) == CTF_ERR)
			return -1;
		btf->ctf_state[id] = DT_BTF_CTF_IDLE;
	}
	return 0;
}

static ctf_dict_t *
dt_btf_to_ctf(dtrace_hdl_t *dtp, dt_module_t *dmp, dt_btf_t *btf)
{
	uint32_t	i, count;
	ctf_dict_t	*ctf;
	ctf_encoding_t	enc = { CTF_INT_SIGNED, 0, 0 };

	/*
	 * Split BTF requires the parent's BTF-to-CTF id mapping.  When shared
	 * CTF comes from an archive, the parent BTF is not converted and its
	 * ids cannot be used in that dictionary.  Leave the module to the
	 * normal CTF fallback rather than mixing the two type sources.
	 */
	if (dmp != NULL && btf != NULL && btf->parent != NULL &&
	    btf->parent->ctfids == NULL) {
		dt_ctf_set_errno(dtp, ECTF_NOTSUP);
		return NULL;
	}

	ctf = ctf_create(&dtp->dt_ctferr);
	if (ctf == NULL)
		return NULL;

	ctf_setmodel(ctf, dtp->dt_conf.dtc_ctfmodel);

	/*
	 * All modules import 'shared_ctf'.  The 'vmlinux' module is handled as
	 * a special case because it simply imports 'shared_ctf'.
	 */
	if (dmp) {
		int	err;

		err = ctf_import(ctf, dt_module_shared_ctf(dtp));
		if (err == CTF_ERR ||
		    ctf_parent_name_set(ctf, "shared_ctf") == CTF_ERR) {
			dt_ctf_error(dtp, ctf);
			goto fail;
		}

		ctf_setspecific(ctf, dmp);

		if (strcmp(dmp->dm_name, "vmlinux") == 0)
			goto out;
	}

	if (!btf)
		goto out;

	/*
	 * If we were not able to read the BTF data, and we are constructing
	 * 'shared_ctf', we need space for the default 'void' type.
	 */
	if (!dmp && !btf->type_cnt)
		btf->type_cnt = 1;

	count = btf->type_cnt ? btf->type_cnt : 1;
	btf->ctfids = dt_calloc(dtp, count, sizeof(ctf_id_t));
	btf->ctf_state = dt_calloc(dtp, count, sizeof(uint8_t));
	btf->ctf_pending = dt_calloc(dtp, count, sizeof(uint32_t));
	if (btf->ctfids == NULL || btf->ctf_state == NULL ||
	    btf->ctf_pending == NULL) {
		dt_set_errno(dtp, EDT_NOMEM);
		goto fail;
	}
	for (i = 0; i < count; i++)
		btf->ctfids[i] = CTF_ERR;

	/*
	 * If we are constructing 'shared_ctf' (no module), we need to add an
	 * entry for the implicit 'void' BTF type (id 0).
	 */
	if (!dmp) {
		btf->ctfids[0] = ctf_add_integer(ctf, CTF_ADD_ROOT, "void",
						 &enc);
		if (btf->ctfids[0] == CTF_ERR)
			goto fail;
	} else
		btf->ctfids[0] = dtp->dt_exec->dm_btf->ctfids[0];

out:
	if (ctf_update(ctf) == CTF_ERR) {
		dt_ctf_error(dtp, ctf);
		goto fail;
	}

	return ctf;

fail:
	ctf_close(ctf);
	if (btf != NULL) {
		dt_free(dtp, btf->ctfids);
		dt_free(dtp, btf->ctf_state);
		dt_free(dtp, btf->ctf_pending);
		btf->ctfids = NULL;
		btf->ctf_state = NULL;
		btf->ctf_pending = NULL;
	}
	return NULL;
}
#endif

ctf_dict_t *
dt_btf_module_ctf(dtrace_hdl_t *dtp, dt_module_t *dmp)
{
	ctf_dict_t	*ctf = NULL;
#ifdef HAVE_LIBCTF
	dt_btf_t	*btf = dmp->dm_btf;
#endif

	/* If we already have CTF data, return it. */
	if (dmp->dm_ctfp)
		return dmp->dm_ctfp;

#ifdef HAVE_LIBCTF
	/*
	 * The shared module owns the dictionary generated from vmlinux BTF.
	 * vmlinux itself, like every other module, imports this dictionary.
	 */
	if (dmp == dtp->dt_exec && dt_module_shared_ctf(dtp) == NULL) {
		dt_module_t *shared = dt_module_create(dtp, "shared_ctf");

		if (shared == NULL) {
			dt_set_errno(dtp, EDT_NOMEM);
			return NULL;
		}
		shared->dm_ctfp = dt_btf_to_ctf(dtp, NULL, btf);
		if (shared->dm_ctfp == NULL)
			return NULL;
		ctf_setspecific(shared->dm_ctfp, shared);
		shared->dm_flags |= DT_DM_LOADED;
	}

	ctf = dt_btf_to_ctf(dtp, dmp, dmp == dtp->dt_exec ? NULL : btf);

	dt_dprintf("Generated %s CTF from BTF (%d types).\n", dmp->dm_name,
		   btf->type_cnt);
#endif

	return ctf;
}

static uint32_t
dt_btf_lookup_name_kind2(const dt_btf_t *btf, int32_t base, const char *name,
			 uint32_t kind)
{
	uint32_t	i;

	if (btf->names == NULL || btf->type_cnt == 0)
		return BTF_TYPE_NONE;

	i = btf->names[str2hval(name, kind) % btf->type_cnt];
	for (; i != 0; i = btf->name_next[i]) {
		const btf_type_t *type = btf->types[i];
		const char *str;

		if (BTF_INFO_KIND(type->info) != kind)
			continue;
		str = dt_btf_get_string(btf, type->name_off);
		if (strcmp(name, str) == 0)
			return i + base;
	}

	return BTF_TYPE_NONE;
}

#ifdef HAVE_LIBCTF
/* Find a CTF-visible name without converting unrelated BTF types. */
static uint32_t
dt_btf_named_type(const dt_btf_t *btf, const char *name)
{
	uint32_t id, kind, base = 0;
	const char *tag = name;

	if (btf->parent != NULL) {
		id = dt_btf_named_type(btf->parent, name);
		if (id != BTF_TYPE_NONE)
			return id;
		if (btf->parent->type_cnt > 0)
			base = btf->parent->type_cnt - 1;
	}

	if (strncmp(name, "struct ", 7) == 0) {
		kind = BTF_KIND_STRUCT;
		tag += 7;
	} else if (strncmp(name, "union ", 6) == 0) {
		kind = BTF_KIND_UNION;
		tag += 6;
	} else if (strncmp(name, "enum ", 5) == 0) {
		kind = BTF_KIND_ENUM;
		tag += 5;
	} else {
		static const uint32_t kinds[] = {
			BTF_KIND_INT, BTF_KIND_FLOAT, BTF_KIND_TYPEDEF
		};
		size_t i;

		for (i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++) {
			id = dt_btf_lookup_name_kind2(btf, base, name,
						    kinds[i]);
			if (id != BTF_TYPE_NONE)
				return id;
		}
		return BTF_TYPE_NONE;
	}

	id = dt_btf_lookup_name_kind2(btf, base, tag, kind);
	if (id != BTF_TYPE_NONE || kind == BTF_KIND_ENUM)
		return id;

	id = dt_btf_lookup_name_kind2(btf, base, tag, BTF_KIND_FWD);
	if (id != BTF_TYPE_NONE) {
		const btf_type_t *type = dt_btf_type_by_id(btf, id);

		if (!!BTF_INFO_KFLAG(type->info) != (kind == BTF_KIND_UNION))
			return BTF_TYPE_NONE;
	}
	return id;
}

/* Discard the cache entries belonging to a failed conversion transaction. */
static void
dt_btf_ctf_abort(dt_btf_t *btf, ctf_dict_t *ctf, ctf_snapshot_id_t snap)
{
	ctf_rollback(ctf, snap);
	while (btf->ctf_pending_cnt > 0) {
		uint32_t id = btf->ctf_pending[--btf->ctf_pending_cnt];

		btf->ctfids[id] = CTF_ERR;
		btf->ctf_state[id] = DT_BTF_CTF_IDLE;
	}
}
#endif

/*
 * All CTF name lookups that can reach kernel BTF go through this helper.
 * Existing CTF dictionaries still use libctf's normal lookup behavior.
 */
ctf_id_t
dt_ctf_lookup_by_name(dtrace_hdl_t *dtp, ctf_file_t *ctf, const char *name)
{
#ifdef HAVE_LIBCTF
	dt_module_t *dmp;
	dt_btf_t *btf = NULL, *parent;
	ctf_snapshot_id_t snap, psnap;
	ctf_id_t id;
	uint32_t bid;
	char base[DT_TYPE_NAMELEN];
	const char *p = name;
	size_t len = 0;
	unsigned int pointers = 0;

	/* vmlinux imports shared types; convert them in the owning parent. */
	if (dtp->dt_exec != NULL && ctf == dtp->dt_exec->dm_ctfp &&
	    !(dtp->dt_exec->dm_flags & DT_DM_CTF_ARCHIVED) &&
	    ctf_parent_dict(ctf) == dt_module_shared_ctf(dtp))
		return dt_ctf_lookup_by_name(dtp, dt_module_shared_ctf(dtp),
					     name);

	if (ctf == dt_module_shared_ctf(dtp) && dtp->dt_exec != NULL)
		btf = dtp->dt_exec->dm_btf;
	else if ((dmp = ctf_getspecific(ctf)) != NULL &&
		 dmp->dm_ctfp == ctf && !(dmp->dm_flags & DT_DM_CTF_ARCHIVED))
		btf = dmp->dm_btf;

	if (btf == NULL || btf->ctfids == NULL)
		return ctf_lookup_by_name(ctf, name);

	id = ctf_lookup_by_name(ctf, name);
	if (id != CTF_ERR && ctf_type_kind(ctf, id) != CTF_K_FORWARD)
		return id;

	/* Extract the named base of libctf's qualified/pointer type syntax. */
	while (*p != '\0') {
		const char *word;
		size_t n;

		if (isspace((unsigned char)*p)) {
			p++;
			continue;
		}
		if (*p == '*') {
			pointers++;
			p++;
			continue;
		}
		word = p;
		while (*p != '\0' && !isspace((unsigned char)*p) && *p != '*')
			p++;
		n = p - word;
		if ((n == 5 && strncmp(word, "const", n) == 0) ||
		    (n == 8 && strncmp(word, "volatile", n) == 0) ||
		    (n == 8 && strncmp(word, "restrict", n) == 0))
			continue;
		if (len + n + 1 >= sizeof(base))
			return ctf_lookup_by_name(ctf, name);
		if (len != 0)
			base[len++] = ' ';
		memcpy(base + len, word, n);
		len += n;
	}
	base[len] = '\0';

	bid = dt_btf_named_type(btf, base);
	if (bid == BTF_TYPE_NONE)
		return ctf_lookup_by_name(ctf, name);

	parent = (dt_btf_t *)btf->parent;
	snap = ctf_snapshot(ctf);
	if (parent != NULL)
		psnap = ctf_snapshot(dt_module_shared_ctf(dtp));

	if (dt_btf_ctf_prepare(dtp, btf, ctf, bid) < 0)
		goto fail;
	id = dt_btf_add_to_ctf(dtp, btf, ctf, bid);
	if (id == CTF_ERR)
		goto fail;

	if (parent != NULL &&
	    dt_btf_ctf_finish(dtp, parent, dt_module_shared_ctf(dtp)) < 0)
		goto fail;
	if (dt_btf_ctf_finish(dtp, btf, ctf) < 0)
		goto fail;

	/* A pointer spelling may be requested before any BTF pointer to it. */
	while (pointers-- > 0) {
		ctf_id_t pointer = ctf_type_pointer(ctf, id);

		if (pointer == CTF_ERR)
			pointer = ctf_add_pointer(ctf, CTF_ADD_ROOT, id);
		id = pointer;
		if (id == CTF_ERR) {
			dt_ctf_error(dtp, ctf);
			goto fail;
		}
	}

	btf->ctf_pending_cnt = 0;
	if (parent != NULL)
		parent->ctf_pending_cnt = 0;
	return ctf_lookup_by_name(ctf, name);

fail:
	dt_btf_ctf_abort(btf, ctf, snap);
	if (parent != NULL)
		dt_btf_ctf_abort(parent, dt_module_shared_ctf(dtp), psnap);
	return CTF_ERR;
#else
	return ctf_lookup_by_name(ctf, name);
#endif
}

uint32_t
dt_btf_lookup_name_kind(dtrace_hdl_t *dtp, dt_module_t *dmp, const char *name,
			uint32_t kind)
{
	dt_btf_t	*btf;
	const dt_btf_t	*parent;
	int32_t		base = 0, id;

	if (kind == BTF_KIND_UNKN)
		return BTF_TYPE_NONE;
	if (strcmp(name, "void") == 0)
		return 0;

	/* Get BTF data for the module. */
	btf = dt_btf_load_module(dtp, dmp);

	/* No type lookup without BTF data. */
	if (!btf)
		return BTF_TYPE_NONE;

	/*
	 * BTF type data for a module is split between two BTF data source:
	 *   shared	[ 0 .. parent->type_cnt - 1 ]
	 *   local	base + [ 1 .. type_cnt ]
	 *   			with base = parent->type_cnt - 1
	 */
	parent = btf->parent;
	if (parent != NULL && parent->type_cnt > 0)
		base = parent->type_cnt - 1;

	id = dt_btf_lookup_name_kind2(btf, base, name, kind);
	if (id == BTF_TYPE_NONE && parent != NULL)
		id = dt_btf_lookup_name_kind2(parent, 0, name, kind);

	return id;
}

/*
 * Retain a descriptor as well as the ID: otherwise an unloaded object's ID
 * could be reused and a later enumeration could mistake it for a cached one.
 * Only IDs assigned to modules are retained, bounding descriptor use by the
 * number of modules discovered by this handle.
 */
typedef struct dt_btf_id {
	dt_hentry_t he;
	uint32_t id;
	int fd;
} dt_btf_id_t;

static uint32_t
dt_btf_id_hval(const dt_btf_id_t *entry)
{
	return entry->id;
}

static int
dt_btf_id_cmp(const dt_btf_id_t *a, const dt_btf_id_t *b)
{
	return a->id != b->id;
}

DEFINE_HE_STD_LINK_FUNCS(dt_btf_id, dt_btf_id_t, he)

static void *
dt_btf_id_release(dt_btf_id_t *head, dt_btf_id_t *entry)
{
	head = dt_btf_id_del(head, entry);
	close(entry->fd);
	free(entry);
	return head;
}

static dt_htab_ops_t dt_btf_id_ops = {
	.hval = (htab_hval_fn)dt_btf_id_hval,
	.cmp = (htab_cmp_fn)dt_btf_id_cmp,
	.add = (htab_add_fn)dt_btf_id_add,
	.del = (htab_del_fn)dt_btf_id_release,
	.next = (htab_next_fn)dt_btf_id_next
};

int
dt_btf_get_module_ids(dtrace_hdl_t *dtp)
{
	uint32_t	id = 0;

	if (dtp->dt_btf_ids == NULL) {
		dtp->dt_btf_ids = dt_htab_create(&dt_btf_id_ops);
		if (dtp->dt_btf_ids == NULL)
			return dt_set_errno(dtp, EDT_NOMEM);
	}

	for (;;) {
		int		rc, fd;
		btf_info_t	info;
		char		name[PATH_MAX];
		uint32_t	size = sizeof(info);
		dt_module_t	*dmp;
		dt_btf_id_t	tmpl, *entry;

		rc = dt_bpf_btf_get_next_id(id, &id);
		if (rc) {
			if (errno == EPERM)
				return 0;	/* Not supported. */
			if (errno == ENOENT)
				return 0;	/* Done. */

			return dt_set_errno(dtp, errno);
		}

		tmpl.id = id;
		if (dt_htab_lookup(dtp->dt_btf_ids, &tmpl) != NULL)
			continue;

		fd = dt_bpf_btf_get_fd_by_id(id);
		if (fd < 0) {
			if (errno == ENOENT)
				continue;	/* Module got unloaded? */

			return dt_set_errno(dtp, errno);
		}

		memset(&info, 0, size);
		info.name = (uint64_t)name;
		info.name_len = sizeof(name);
		rc = dt_bpf_btf_get_info_by_fd(fd, &info, &size);
		if (rc) {
			int err = errno;

			close(fd);
			return dt_set_errno(dtp, err);
		}

		dmp = dt_module_lookup_by_name(dtp, name);
		if (!info.kernel_btf || dmp == NULL || dmp->dm_btf_id != 0) {
			close(fd);
			continue;
		}

		entry = calloc(1, sizeof(*entry));
		if (entry == NULL) {
			close(fd);
			return dt_set_errno(dtp, EDT_NOMEM);
		}
		entry->id = id;
		entry->fd = fd;
		if (dt_htab_insert(dtp->dt_btf_ids, entry) != 0) {
			close(fd);
			free(entry);
			return dt_set_errno(dtp, EDT_NOMEM);
		}
		dmp->dm_btf_id = id;
	}
}

int
dt_btf_module_fd(const dt_module_t *dmp)
{
	return dmp->dm_btf_id ? dt_bpf_btf_get_fd_by_id(dmp->dm_btf_id) : 0;
}

int
dt_btf_func_argc(const dt_btf_t *btf, uint32_t id)
{
	btf_type_t	*type = dt_btf_real_type_by_id(btf, id);

	if (type == NULL)
		return -1;

	if (BTF_INFO_KIND(type->info) != BTF_KIND_FUNC_PROTO)
		return -1;

	return BTF_INFO_VLEN(type->info);
}

int
dt_btf_func_is_void(const dt_btf_t *btf, uint32_t id)
{
	btf_type_t	*type = dt_btf_real_type_by_id(btf, id);

	if (type != NULL &&
	    BTF_INFO_KIND(type->info) == BTF_KIND_FUNC_PROTO && type->type == 0)
		return 1;

	return 0;
}

/*
 * Return 1 if the function referenced by the BTF id can be traced using the
 * fprobe/fexit facility.  Specifically, that the function is not variadic and
 * that none of its arguments exceeds the acceptable value size (16 bytes).
 * (See btf_distill_func_proto() in kernel/bpf/btf.c for reference.)
 */
#define MAX_BPF_FUNC_REG_REGS	5
int
dt_btf_func_is_traceable(const dt_btf_t *btf, uint32_t id)
{
	btf_type_t	*type = dt_btf_real_type_by_id(btf, id);
	int		argc;

	/* If no prototype is found, BPF fprobes do not work. */
	if (!type || BTF_INFO_KIND(type->info) != BTF_KIND_FUNC_PROTO)
		return 0;

	/* If the return type is a struct or union, BPF fprobes do not work. */
	if (type->type != 0) {
		btf_type_t	*rtype = dt_btf_real_type_by_id(btf, type->type);

		if (rtype == NULL)
			return 0;

		switch (BTF_INFO_KIND(rtype->info)) {
		case BTF_KIND_STRUCT:
		case BTF_KIND_UNION:
			return 0;
		}
	}

	/*
	 * BPF fprobes do not support functions that are variadic or that have
	 * any arguments passed by value that are a struct or union of size
	 * greater than 16.
	 */
	argc = BTF_INFO_VLEN(type->info);
	if (argc > 0) {
		btf_param_t	*args = (btf_param_t *)(type + 1);
		int		i;

		if (args[argc - 1].type == 0)		/* variadic */
			return 0;

		for (i = 0; i < argc; i++) {
			type = dt_btf_real_type_by_id(btf, args[i].type);
			if (type == NULL)
				return 0;

			switch (BTF_INFO_KIND(type->info)) {
			case BTF_KIND_STRUCT:
			case BTF_KIND_UNION:
				if (type->size > 16)	/* value size > 16 */
					return 0;
			}
		}
	}

	return 1;
}
