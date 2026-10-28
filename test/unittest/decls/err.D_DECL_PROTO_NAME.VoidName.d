/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * Assert that a void parameter in a declaration where void is permitted
 * may not have a parameter formal name associated with it.
 */

int a(void v);

dtrace:::BEGIN,
dtrace:::ERROR
{
	exit(0);
}
