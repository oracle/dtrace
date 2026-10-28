/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

/* @@xfail: not yet ported */

sdt:::callout-start
{
	@callouts[((callout_t *)arg0)->c_func] = count();
}

profile:::tick-1sec
{
	printa("%40a %10@d\n", @callouts);
	clear(@callouts);
}
