/*
 * Oracle Linux DTrace.
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: An explicitly-scoped userspace symbol reports a lookup error
 * when its module cannot be found.
 */

#pragma D option quiet

dtrace:::BEGIN
{
	trace(no_such_module``main);
	exit(0);
}
