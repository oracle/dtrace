/*
 * Oracle Linux DTrace.
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: D scoping operators are not permitted in declaration names.
 */

#pragma D option quiet

int foo`bar;

dtrace:::BEGIN
{
	exit(1);
}
