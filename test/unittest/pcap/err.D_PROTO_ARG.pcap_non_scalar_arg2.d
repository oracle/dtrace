/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The second argument to pcap() should be a scalar.
 *
 * SECTION: Actions and Subroutines/pcap()
 */

dtrace:::BEGIN
{
	pcap(0, "a");
	exit(0);
}

dtrace:::ERROR
{
	exit(0);
}
