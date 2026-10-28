/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * Ensure that we do not leak tstrings with void expressions.
 */

#pragma D option quiet

dtrace:::BEGIN {
	strjoin("a", "b");
	strjoin("a", "b");
	strjoin("a", "b");
	strjoin("a", "b");
	strjoin("a", "b");
	strjoin("a", "b");
	strjoin("a", "b");
	strjoin("a", "b");
	strjoin("a", "b");
	strjoin("a", "b");
	strjoin("a", "b");
	strjoin("a", "b");
	strjoin("a", "b");
	strjoin("a", "b");
	strjoin("a", "b");
	strjoin("a", "b");
	exit(0);
}

dtrace:::ERROR {
	exit(1);
}
