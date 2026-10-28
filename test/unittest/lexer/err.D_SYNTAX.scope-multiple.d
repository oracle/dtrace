/*
 * Oracle Linux DTrace.
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Multiple D scoping operators in one identifier are a syntax
 * error.
 */

#pragma D option quiet

dtrace:::BEGIN
{
	trace(foo``bar``baz);
	exit(0);
}
