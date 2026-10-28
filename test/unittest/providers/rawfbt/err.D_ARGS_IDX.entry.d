/*
 * Oracle Linux DTrace.
 * Copyright (c) 2024, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Accessing args[] for rawfbt entry probes raises an error.
 */

#pragma D option quiet

rawfbt:vmlinux:exit_creds:entry
{
	trace(args[0]);
	exit(0);
}

dtrace:::ERROR
{
	exit(1);
}
