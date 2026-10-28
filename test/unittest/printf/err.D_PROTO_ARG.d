/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Test printf() with a bad format string arg.
 *
 * SECTION: Output Formatting/printf()
 *
 */

dtrace:::BEGIN
{
	printf(123);
}
