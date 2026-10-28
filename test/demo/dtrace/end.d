/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: none */

dtrace:::BEGIN
{
	start = timestamp;
}

profile:::tick-1s
{
	exit(0);
}

dtrace:::END
{
	printf("end reached");
}
