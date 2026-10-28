/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* Check 'caller'. */

#pragma D option quiet

dtrace:::BEGIN {
	trace(caller);
	exit(0);
}

dtrace:::ERROR
{
	printf("error encountered\n");
	exit(1);
}
