/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

/*
 * This test verifies that the height of the ASCII quantization bars is
 * determined using rounding and not truncated integer arithmetic.
 */
profile:::tick-10ms
/i++ >= 27/
{
	exit(0);
}

profile:::tick-10ms
/i > 8/
{
	@ = lquantize(2, 0, 4);
}

profile:::tick-10ms
/i > 2 && i <= 8/
{
	@ = lquantize(1, 0, 4);
}

profile:::tick-10ms
/i <= 2/
{
	@ = lquantize(0, 0, 4);
}
