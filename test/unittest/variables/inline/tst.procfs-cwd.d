/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The 'cwd' inline variable returns the default value.
 */

#pragma D option quiet

dtrace:::BEGIN
{
	trace(cwd);
}

dtrace:::BEGIN
/cwd == "<unknown>"/
{
	exit(0);
}

dtrace:::BEGIN,
dtrace:::ERROR {
	exit(1);
}
