/*
 * Oracle Linux DTrace.
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: strlen() must not overwrite the IPv6 lookup table in the
 *            rodata block of the strtab map.
 *
 * SECTION: Actions and Subroutines/strlen()
 */
/* @@nosort */

#pragma D option quiet
#pragma D option strsize=128

BEGIN
{
	this->addr = (struct in6_addr *)alloca(sizeof(struct in6_addr));
	this->addr->in6_u.u6_addr32[0] = 0;
	this->addr->in6_u.u6_addr32[1] = 0;
	this->addr->in6_u.u6_addr32[2] = 0;
	this->addr->in6_u.u6_addr32[3] = 0;
	printf("before: %s\n", inet_ntoa6(this->addr));

	/*
	 * Reach past any alignment padding and the reserved rodata byte into
	 * the first lookup table entries, without reaching the zeros block.
	 */
	printf("length: %d\n", strlen(
	    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"));
	printf("after: %s\n", inet_ntoa6(this->addr));
	exit(0);
}

ERROR
{
	exit(1);
}
