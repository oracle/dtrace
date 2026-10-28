/*
 * Oracle Linux DTrace.
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: testprobe */
/* @@runtest-opts: -e -xlinkmode=static */

/*
 * ASSERTION: Explicit and implicit userspace type scopes, and the implicit
 * kernel type scope, resolve external types.
 */

#pragma D option quiet

dtrace:::BEGIN
{
	scope_explicit = (struct testprobe``dtrace_scope_test_type *)NULL;
	scope_implicit = (struct ``dtrace_scope_test_type *)NULL;
	scope_kernel = (struct `task_struct *)NULL;
	exit(0);
}
