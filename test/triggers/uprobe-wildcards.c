/*
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dt_module.h>
#include <dt_probe.h>
#include <dt_provider.h>

#define CHECK(expr) do { \
	if (!(expr)) { \
		fprintf(stderr, "Check failed at line %d: %s\n", __LINE__, #expr); \
		return 1; \
	} \
} while (0)

/* Export these to exercise both .symtab and stripped .dynsym objects. */
int uprobe_test_data;
int uprobe_test_alpha(void) { return 1; }
int uprobe_test_beta(void) { return 2; }

static int
count_probe(dtrace_hdl_t *dtp, const dtrace_probedesc_t *pdp, void *arg)
{
	int *count = arg;

	(*count)++;
	return 0;
}

typedef struct module_count {
	int count;
	int result;
} module_count_t;

static int
count_module(dtrace_hdl_t *dtp, dt_module_t *dmp, void *arg)
{
	module_count_t *data = arg;

	data->count++;
	return data->result;
}

int
main(int argc, char **argv)
{
	dtrace_hdl_t	*dtp;
	dt_probe_t	*prp;
	int		err, count = 0;
	module_count_t	modules = { 0 };
	dtrace_probedesc_t pd = {
		.prv = "uprobe", .mod = "uprobe-wildcards-a",
		.fun = "uprobe_test_*", .prb = "*",
	};

	CHECK(argc == 2);
	CHECK(setenv("LD_LIBRARY_PATH", argv[1], 1) == 0);
	CHECK((dtp = dtrace_open(DTRACE_VERSION, 0, &err)) != NULL);
	dt_probe_init(dtp);
	CHECK(dt_uprobe.populate(dtp) == 0);

	CHECK(dt_uprobe.provide(dtp, &pd) == 4);
	CHECK(dt_uprobe.provide(dtp, &pd) == 0);
	pd.mod = "uprobe-wildcards-[ab]";
	CHECK(dt_uprobe.provide(dtp, &pd) == 4);
	pd.mod = "uprobe-wildcards-?";
	CHECK(dt_uprobe.provide(dtp, &pd) == 0);
	pd.prv = "uprob?";
	pd.fun = "uprobe_test_?lpha";
	pd.prb = "[er]*";
	CHECK(dtrace_probe_iter(dtp, &pd, count_probe, &count) == 0);
	CHECK(count == 4);
	pd.prv = "uprobe";

	/* Visit cached modules once each and honor callback termination. */
	CHECK(dt_module_user_iter(dtp, "uprobe-wildcards-[ab]",
	    count_module, &modules) == 0);
	CHECK(modules.count == 2);
	modules.count = 0;
	modules.result = 7;
	CHECK(dt_module_user_iter(dtp, "uprobe-wildcards-[ab]",
	    count_module, &modules) == 7);
	CHECK(modules.count == 1);
	modules.count = 0;
	CHECK(dt_module_user_iter(dtp, "uprobe-no-such-module-*",
	    count_module, &modules) == 0);
	CHECK(modules.count == 0);

	pd.mod = "uprobe-wildcards-b";
	pd.fun = "uprobe_test_alpha";
	pd.prb = "entry";
	CHECK((prp = dt_probe_lookup(dtp, &pd)) != NULL);
	CHECK(strcmp(prp->desc->mod, pd.mod) == 0);
	CHECK(strcmp(prp->desc->fun, pd.fun) == 0);
	pd.prb = "return";
	CHECK(dt_probe_lookup(dtp, &pd) != NULL);
	pd.fun = "uprobe_test_data";
	CHECK(dt_uprobe.provide(dtp, &pd) == 0);
	CHECK(dt_probe_lookup(dtp, &pd) == NULL);
	pd.fun = "fprintf";
	CHECK(dt_uprobe.provide(dtp, &pd) == 0);
	pd.prb = "no-such-probe";
	CHECK(dt_uprobe.provide(dtp, &pd) == 0);

	/* Empty components match everything, including both probe names. */
	pd.mod = "uprobe-wildcards-b";
	pd.fun = "";
	pd.prb = "";
	CHECK(dt_uprobe.provide(dtp, &pd) > 0);
	CHECK(dt_uprobe.provide(dtp, &pd) == 0);
	pd.mod = "";
	pd.fun = "uprobe_test_?lpha";
	CHECK(dt_uprobe.provide(dtp, &pd) >= 0);

	dtrace_close(dtp);
	return 0;
}
