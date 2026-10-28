/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The 'tid' variable can be accessed and is not -1.
 *
 * SECTION: Variables/Built-in Variables/tid
 */

#pragma D option quiet

dtrace:::BEGIN {
	trace(tid);
	exit(tid != -1 ? 0 : 1);
}

dtrace:::ERROR {
	exit(1);
}
