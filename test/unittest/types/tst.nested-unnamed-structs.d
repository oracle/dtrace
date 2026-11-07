/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Check for the absence of a libctf bug breaking offsets of
 *	members of nested unnamed structs in the kernel.
 *	See https://sourceware.org/bugzilla/show_bug.cgi?id=30264
 *
 */

#pragma D option quiet

dtrace:::BEGIN {
	printf("offset is %i\n", offsetof(struct `flow_dissector_key_icmp, code));
}

dtrace:::BEGIN
/ offsetof(struct `flow_dissector_key_icmp, code) == 0 /
{
	exit(1);
}

dtrace:::BEGIN
/ offsetof(struct `flow_dissector_key_icmp, code) != 0 /
{
	exit(0);
}
