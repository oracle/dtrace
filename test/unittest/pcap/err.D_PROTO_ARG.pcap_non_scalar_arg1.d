/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The first argument to pcap() should be a pointer.
 *
 * SECTION: Actions and Subroutines/pcap()
 */

struct {
	int	a;
} arg;

dtrace:::BEGIN
{
	pcap(arg, 0);
	exit(0);
}

dtrace:::ERROR
{
	exit(0);
}
