/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The 'epid' variable can be accessed and is not -1.
 *
 * SECTION: Variables/Built-in Variables/epid
 */

#pragma D option quiet

dtrace:::BEGIN {
	trace(epid);
	exit(epid != -1 ? 0 : 1);
}

dtrace:::ERROR {
	exit(1);
}
