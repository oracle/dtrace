/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Declare self a variable and assign appropriate value.
 *
 * SECTION:  Variables/Scalar Variables
 *
 */
self x;

dtrace:::BEGIN
{
	x = "dummy";
	exit(0);
}
