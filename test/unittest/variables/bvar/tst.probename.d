/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The 'probename' variable returns the correct value.
 *
 * SECTION: Variables/Built-in Variables/probename
 */

#pragma D option quiet

dtrace:::BEGIN
{
	trace(probename);
}

dtrace:::BEGIN
/probename == "BEGIN"/
{
	exit(0);
}

dtrace:::BEGIN
{
	exit(1);
}

dtrace:::ERROR {
	exit(1);
}
