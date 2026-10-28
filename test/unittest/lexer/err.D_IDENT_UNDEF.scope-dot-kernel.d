/*
 * Oracle Linux DTrace.
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Dotted kernel module scopes lex as a single external symbol
 * reference.
 */

#pragma D option quiet

dtrace:::BEGIN
{
	trace(foo.bar`baz);
	exit(0);
}
