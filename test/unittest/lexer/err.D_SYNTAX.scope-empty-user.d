/*
 * Oracle Linux DTrace.
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: A userspace D scoping operator with an empty identifier is a
 * syntax error.
 */

#pragma D option quiet

dtrace:::BEGIN
{
	trace(bar``);
	exit(0);
}
