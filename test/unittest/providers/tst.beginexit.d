/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

dtrace:::BEGIN
{
	exit(0);
}

dtrace:::BEGIN
{
	printf("shouldn't be here...");
	here++;
}

dtrace:::END
{
	exit(here);
}
