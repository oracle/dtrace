/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  stringof() accepts a scalar, pointer, or string
 *
 * SECTION: Types, Operators, and Expressions/Precedence
 *
 */

#pragma D option quiet

dtrace:::BEGIN
{
	printf("%s", stringof (trace(0)));
	exit(1);
}
