/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * Test that a strtok(NULL, ...) without first calling strtok(string, ...)
 * produces an error
 */

dtrace:::BEGIN
{
	trace(strtok(NULL, "!"));
	exit(1);
}

dtrace:::ERROR
{
	exit(0);
}
