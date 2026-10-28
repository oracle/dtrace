/*
 * Oracle Linux DTrace.
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: More than two consecutive D scoping operator characters are a
 * syntax error.
 */

#pragma D option quiet

dtrace:::BEGIN
{
	trace(foo```bar);
	exit(0);
}
