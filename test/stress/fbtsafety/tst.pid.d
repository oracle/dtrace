/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	collect pid at every fbt probe and at every firing of a
 *	high-frequency profile probe
 */

fbt:::
{
	@a[pid] = count();
}

profile:::profile-4999hz
{
	@a[pid] = count();
}

profile:::tick-1sec
/n++ == 10/
{
	exit(0);
}
