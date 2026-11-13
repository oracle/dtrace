/*
 * Oracle Linux DTrace.
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: testprobe */
/* @@runtest-opts: -xlinkmode=static */

/*
 * ASSERTION: Explicit userspace module scoping resolves as a userspace
 * external symbol reference.
 */

#pragma D option quiet

dtrace:::BEGIN
{
	trace(&testprobe``main != NULL);
	exit(0);
}
