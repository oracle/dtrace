/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet
#pragma D option strsize=14

dtrace:::BEGIN
{
	printf("|%s|\n", substr((char *)&`linux_banner, 1));
	printf("|%s|\n", substr((char *)&`linux_banner, 1, 4));
	exit(0);
}

dtrace:::ERROR
{
	exit(1);
}
