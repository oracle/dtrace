/*
 * Oracle Linux DTrace.
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * offsetof() accepts a member name that is also a typedef name.  The kernel
 * defines both the code typedef and flow_dissector_key_icmp.code, which is
 * inside an anonymous struct.
 */

#pragma D option quiet

dtrace:::BEGIN
{
	printf("offset is %d\n", offsetof(struct `flow_dissector_key_icmp, code));
	exit(offsetof(struct `flow_dissector_key_icmp, code) != 1);
}
