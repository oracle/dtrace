/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

profile:::tick-10ms
/i++ < 10/
{
	@ = lquantize(i, 0, 10);
	@ = lquantize(i, 0, 10);
	@ = lquantize(i, 0, 10);
	@ = lquantize(i, 0, 10);
	@ = lquantize(i, 0, 10);
}

profile:::tick-10ms
/i == 10/
{
	exit(0);
}

dtrace:::END
{
	normalize(@, 5);
	printa(@);
}
