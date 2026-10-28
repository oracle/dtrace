/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Quantization step must be an integer constant.
 *
 * SECTION: Aggregations/Aggregations
 */

dtrace:::BEGIN
{
	steps = 2;
	@ = lquantize(1, 0, 1100, steps);
	exit(0);
}
