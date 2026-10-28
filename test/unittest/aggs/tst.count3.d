/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	Test multiple count() calls.
 *
 * SECTION: Aggregations/Aggregations
 *
 */

#pragma D option quiet

dtrace:::BEGIN
{
	i = 0;
}

profile:::tick-10ms
/i != 10/
{
	i++;
	@counts1[execname] = count();
	@counts2[execname, arg0] = count();
}

profile:::tick-10ms
/i == 10/
{
	exit(0);
}
