/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * This test confirms the orthogonality of associative arrays and thread-local
 * variables by intentionally deriving a matching key signature (based on
 * t_did).
 */
uint64_t b[uint64_t];

dtrace:::BEGIN
{
	self->a = 0xbad;
}

dtrace:::BEGIN
/b[(uint64_t)curthread] == 0/
{
	exit(0);
}

dtrace:::BEGIN
{
	printf("value should be 0; value is %x!", b[(uint64_t)curthread]);
	exit(1);
}
