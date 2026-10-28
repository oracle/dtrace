/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Char arrays can be used a key components.
 *
 * SECTION: Aggregations/Aggregations
 */

#pragma D option quiet

dtrace:::BEGIN
{
	@[(string)curpsinfo->pr_fname] = count();
	exit(0);
}

dtrace:::ERROR
{
	exit(1);
}
