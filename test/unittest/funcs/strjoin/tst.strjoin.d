/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

dtrace:::BEGIN
{
	printf("%s\n", strjoin("foo", "baz"));
	printf("%s\n", strjoin("foo", ""));
	printf("%s\n", strjoin("", "baz"));
	printf("%s\n", strjoin("", ""));
	exit(0);
}
