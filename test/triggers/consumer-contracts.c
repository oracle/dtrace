/*
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* Include the private aggregate layout to seed data without BPF maps. */
#include "libdtrace/dt_aggregate.c"
#include "libdtrace/dt_consume.c"

#define CHECK(x) do { if (!(x)) { \
	fprintf(stderr, "line %d: %s\n", __LINE__, #x); exit(1); \
} } while (0)

static int drop(const dtrace_dropdata_t *d, void *arg) { return 0; }
static int error(const dtrace_errdata_t *d, void *arg) { return 0; }
static void proc(pid_t pid, const char *s, void *arg) { }
static int buffered(const dtrace_bufdata_t *d, void *arg) { return 0; }
static int setopt(const dtrace_setoptdata_t *d, void *arg) { return 0; }

static void
handlers(void)
{
	dtrace_hdl_t dt = { 0 };
	int first, second;

	CHECK(dtrace_handle_drop(&dt, drop, &first) == 0);
	CHECK(dtrace_handle_drop(&dt, drop, &second) == -1);
	CHECK(dtrace_errno(&dt) == EALREADY && dt.dt_droparg == &first);
	CHECK(dtrace_handle_proc(&dt, proc, &first) == 0);
	CHECK(dtrace_handle_proc(&dt, proc, &second) == -1);
	CHECK(dtrace_errno(&dt) == EALREADY && dt.dt_procarg == &first);
	CHECK(dtrace_handle_buffered(&dt, NULL, &first) == -1);
	CHECK(dtrace_errno(&dt) == EINVAL && dt.dt_bufhdlr == NULL);
	CHECK(dtrace_handle_buffered(&dt, buffered, &first) == 0);
	CHECK(dtrace_handle_buffered(&dt, buffered, &second) == -1);
	CHECK(dtrace_errno(&dt) == EALREADY && dt.dt_bufarg == &first);
	CHECK(dtrace_handle_setopt(&dt, NULL, &first) == -1);
	CHECK(dtrace_errno(&dt) == EINVAL && dt.dt_setopthdlr == NULL);
	CHECK(dtrace_handle_setopt(&dt, setopt, &first) == 0);
	CHECK(dtrace_handle_setopt(&dt, setopt, &second) == 0);
	CHECK(dt.dt_setoptarg == &second);
	/* GRABANON registers the error handler without compiling an ERROR probe. */
	dt.dt_options[DTRACEOPT_GRABANON] = 1;
	CHECK(dtrace_handle_err(&dt, error, &first) == 0);
	CHECK(dtrace_handle_err(&dt, error, &second) == -1);
	CHECK(dtrace_errno(&dt) == EALREADY && dt.dt_errarg == &first);
	/* Repeated go and stop calls must not create kernel resources. */
	dt.dt_active = 1;
	CHECK(dtrace_go(&dt, 0) == -1 && dtrace_errno(&dt) == EINVAL);
	dt.dt_stopped = 1;
	CHECK(dtrace_stop(&dt) == 0 && dtrace_stop(&dt) == 0);
}

struct walk {
	int calls;
	int result;
	int remove_second;
};

static dtrace_datadesc_t callback_desc;

/* Supply a decoded statement; leave record dispatch and callbacks real. */
int
__wrap_dt_stid_lookup(dtrace_hdl_t *dtp, dtrace_stid_t stid,
		     dtrace_datadesc_t **desc)
{
	if (stid != 7)
		return -1;
	*desc = &callback_desc;
	return 0;
}

struct callbacks {
	int probe_result, record_result;
	int probes, records, ends;
};

static int
probe_callback(const dtrace_probedata_t *data, void *arg)
{
	struct callbacks *cb = arg;
	cb->probes++;
	CHECK(data->dtpda_stid == 7);
	return cb->probe_result;
}

static int
record_callback(const dtrace_probedata_t *data, const dtrace_recdesc_t *rec,
		void *arg)
{
	struct callbacks *cb = arg;
	if (rec == NULL) {
		cb->ends++;
		return DTRACE_CONSUME_THIS;
	}
	cb->records++;
	CHECK(*(uint64_t *)data->dtpda_data == 42);
	return cb->record_result;
}

static void
consumption(void)
{
	dtrace_hdl_t dt = { 0 };
	dtrace_probedesc_t desc = { .prv = "test", .mod = "module",
		.fun = "function", .prb = "entry" };
	dt_probe_t probe = { .desc = &desc }, *probes[] = { NULL, &probe };
	dtrace_recdesc_t rec = { .dtrd_action = DTRACEACT_DIFEXPR,
		.dtrd_size = 8, .dtrd_offset = DBUF_DATA, .dtrd_alignment = 8 };
	uint64_t buffer[3] = { 0 };
	struct testcase {
		int probe_result, record_result, result, error, records, ends;
	} cases[] = {
		{ DTRACE_CONSUME_THIS, DTRACE_CONSUME_NEXT, DTRACE_WORKSTATUS_OKAY, 0, 1, 1 },
		{ DTRACE_CONSUME_NEXT, DTRACE_CONSUME_THIS, DTRACE_WORKSTATUS_OKAY, 0, 0, 0 },
		{ DTRACE_CONSUME_DONE, DTRACE_CONSUME_THIS, DTRACE_WORKSTATUS_DONE, 0, 0, 0 },
		{ DTRACE_CONSUME_ABORT, DTRACE_CONSUME_THIS, DTRACE_WORKSTATUS_ERROR, EDT_DIRABORT, 0, 0 },
		{ 999, DTRACE_CONSUME_THIS, DTRACE_WORKSTATUS_ERROR, EDT_BADRVAL, 0, 0 },
		{ DTRACE_CONSUME_THIS, DTRACE_CONSUME_ABORT, DTRACE_WORKSTATUS_ERROR, EDT_DIRABORT, 1, 0 },
		{ DTRACE_CONSUME_THIS, 999, DTRACE_WORKSTATUS_ERROR, EDT_BADRVAL, 1, 0 },
	};
	FILE *fp = tmpfile();
	size_t i;

	CHECK(fp != NULL);
	dt.dt_probes = probes;
	dt.dt_probe_id = 2;
	callback_desc.dtdd_nrecs = 1;
	callback_desc.dtdd_recs = &rec;
	*(dtrace_id_t *)((char *)buffer + DBUF_PRID) = 1;
	*(dtrace_stid_t *)((char *)buffer + DBUF_STID) = 7;
	buffer[2] = 42;
	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		struct callbacks cb = { .probe_result = cases[i].probe_result,
			.record_result = cases[i].record_result };
		dtrace_probedata_t data = { .dtpda_handle = &dt };
		dtrace_id_t last_probe = 0;
		dtrace_stid_t last_stmt = 0;

		dt.dt_errno = 0;
		CHECK(dt_consume_one_probe(&dt, fp, (char *)buffer, sizeof(buffer),
		    &data, probe_callback, record_callback, 0, 1, 0,
		    &last_probe, &last_stmt, 0, &cb) == cases[i].result);
		CHECK(cb.probes == 1 && cb.records == cases[i].records && cb.ends == cases[i].ends);
		CHECK(dtrace_errno(&dt) == cases[i].error);
		if (cb.ends)
			CHECK(last_probe == 1 && last_stmt == 7);
	}
	fclose(fp);
}

static int
visit(const dtrace_aggdata_t *data, void *arg)
{
	struct walk *walk = arg;
	walk->calls++;
	if (walk->result == DTRACE_AGGWALK_ERROR)
		errno = EIO;
	if (walk->remove_second)
		return walk->calls == 2 ? DTRACE_AGGWALK_REMOVE : DTRACE_AGGWALK_NEXT;
	return walk->result;
}

static void
aggregation(void)
{
	dtrace_hdl_t dt = { 0 };
	dt_aggregate_t aggregate = { 0 };
	dt_ahashent_t *entries[3], *bucket = NULL;
	dtrace_aggdesc_t desc = { .dtagd_ksize = 8 };
	char key[8];
	struct walk walk;
	int i;

	dt.dt_aggregate = &aggregate;
	dt.dt_maxtuplesize = sizeof(key);
	aggregate.dtat_key = key;
	aggregate.dtat_hash.dtah_hash = &bucket;
	aggregate.dtat_hash.dtah_size = 1;
	walk = (struct walk) { .result = DTRACE_AGGWALK_NEXT };
	CHECK(dtrace_aggregate_walk(&dt, visit, &walk) == 0 && walk.calls == 0);
	for (i = 0; i < 3; i++) {
		CHECK((entries[i] = calloc(1, sizeof(*entries[i]))) != NULL);
		entries[i]->dtahe_data.dtada_desc = &desc;
		CHECK((entries[i]->dtahe_data.dtada_key = calloc(1, sizeof(key))) != NULL);
		CHECK((entries[i]->dtahe_data.dtada_data = calloc(1, 8)) != NULL);
		CHECK((entries[i]->dtahe_data.dtada_percpu = calloc(1, sizeof(char *))) != NULL);
		CHECK((entries[i]->dtahe_data.dtada_percpu[0] = calloc(1, 8)) != NULL);
		if (i > 0) {
			entries[i - 1]->dtahe_next = entries[i - 1]->dtahe_nextall = entries[i];
			entries[i]->dtahe_prev = entries[i]->dtahe_prevall = entries[i - 1];
		}
	}
	bucket = aggregate.dtat_hash.dtah_all = entries[0];
	for (i = 0; i < 4; i++) {
		int results[] = { DTRACE_AGGWALK_NEXT, DTRACE_AGGWALK_ABORT,
			DTRACE_AGGWALK_ERROR, 999 };
		int errors[] = { 0, EDT_DIRABORT, 0, EDT_BADRVAL };
		walk = (struct walk) { .result = results[i] };
		CHECK(dtrace_aggregate_walk(&dt, visit, &walk) == (i == 0 ? 0 : -1));
		CHECK(walk.calls == (i == 0 ? 3 : 1));
		if (errors[i])
			CHECK(dtrace_errno(&dt) == errors[i]);
		if (i == 2)
			CHECK(errno == EIO);
		CHECK(bucket == entries[0] && entries[0]->dtahe_next == entries[1]);
	}
	/* Remove a middle collision-chain member, then the head and tail. */
	walk = (struct walk) { .remove_second = 1 };
	CHECK(dtrace_aggregate_walk(&dt, visit, &walk) == 0 && walk.calls == 3);
	CHECK(entries[0]->dtahe_next == entries[2] && entries[2]->dtahe_prev == entries[0]);
	CHECK(entries[0]->dtahe_nextall == entries[2] && entries[2]->dtahe_prevall == entries[0]);
	walk = (struct walk) { .result = DTRACE_AGGWALK_REMOVE };
	CHECK(dtrace_aggregate_walk(&dt, visit, &walk) == 0 && walk.calls == 2);
	CHECK(bucket == NULL && aggregate.dtat_hash.dtah_all == NULL);
	walk.calls = 0;
	CHECK(dtrace_aggregate_walk(&dt, visit, &walk) == 0 && walk.calls == 0);
}

int
main(void)
{
	handlers();
	aggregation();
	consumption();
	return 0;
}
