/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: none */

dtrace:::BEGIN
{
	*(char *)NULL;
}

dtrace:::ERROR
{
	printf("Hit an error!");
	exit(0);
}
