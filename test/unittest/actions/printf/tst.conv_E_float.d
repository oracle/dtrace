/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The printf action supports '%E' for floating point values.
 *
 * SECTION: Actions/printf()
 */

#pragma D option quiet

float f;

dtrace:::BEGIN
{
	printf("%E", f);
	exit(0);
}
