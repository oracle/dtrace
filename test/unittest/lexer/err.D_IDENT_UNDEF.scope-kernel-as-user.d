/*
 * Oracle Linux DTrace.
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Kernel symbols cannot be referenced using userspace scoping.
 */

#pragma D option quiet

dtrace:::BEGIN
{
	trace(vmlinux``major_names);
	exit(0);
}
