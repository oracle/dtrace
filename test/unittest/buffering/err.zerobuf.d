/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *   Test with a bufsize of 0 - should return an error.
 *
 * SECTION:
 *	Buffers and Buffering/Buffer Sizes;
 *	Options and Tunables/bufsize
 */

#pragma D option bufsize=0

dtrace:::BEGIN
{
	exit(0);
}
