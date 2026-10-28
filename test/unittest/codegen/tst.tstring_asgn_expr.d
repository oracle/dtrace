/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * Ensure tstrings are handled correctly for non-void assignment expressions.
 */

#pragma D option quiet

dtrace:::BEGIN {
	trace(z = strjoin((x = strjoin("abc", "def")),
			  (y = strjoin("ABC", "DEF"))));
	trace(" ");
	trace(x);
	trace(" ");
	trace(y);
	trace(" ");
	trace(z);

	exit(0);
}

dtrace:::ERROR {
	exit(1);
}
