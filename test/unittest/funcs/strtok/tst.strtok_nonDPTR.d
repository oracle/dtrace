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
	printf("|%s|\n", strtok((char *)&`linux_banner, " "));
	printf("|%s|\n", strtok(NULL, " "));
	printf("|%s|\n", strtok("@@@@@ !!!!!", (char *)&`linux_banner));
	printf("|%s|\n", strtok(NULL, (char *)&`linux_banner));
	exit(0);
}

dtrace:::ERROR
{
	exit(1);
}
