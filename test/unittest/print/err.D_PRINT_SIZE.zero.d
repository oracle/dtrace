/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

dtrace:::BEGIN
{
	t = (int *)alloca(sizeof(int));
	print((struct bpf_raw_tracepoint_args *)t);
	exit(0);
}
