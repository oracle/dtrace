/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	collect ucaller at every fbt probe and at every firing of a
 *	high-frequency profile probe
 */

fbt:::
{
	@a[ucaller] = count();
}

profile:::profile-4999hz
{
	@a[ucaller] = count();
}

profile:::tick-1sec
/n++ == 30/
{
	exit(0);
}
