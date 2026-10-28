/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@nosort */

#pragma D option strsize=2k
#pragma D option bufsize=64k

dtrace:::BEGIN
{
	exit(0);
}

dtrace:::END
{
	trace("00");
	trace("01");
}

dtrace:::END
{
	trace("02");
	trace("03");
}

dtrace:::END
{
	trace("04");
	trace("05");
}

dtrace:::END
{
	trace("06");
	trace("07");
}

dtrace:::END
{
	trace("08");
	trace("09");
}

dtrace:::END
{
	trace("10");
	trace("11");
}

dtrace:::END
{
	trace("12");
	trace("13");
}

dtrace:::END
{
	trace("14");
	trace("15");
}

dtrace:::END
{
	trace("16");
	trace("17");
}

dtrace:::END
{
	trace("18");
	trace("19");
}

dtrace:::END
{
	trace("20");
	trace("21");
}

dtrace:::END
{
	trace("22");
	trace("23");
}

dtrace:::END
{
	trace("24");
	trace("25");
}

dtrace:::END
{
	trace("26");
	trace("27");
}

dtrace:::END
{
	trace("28");
	trace("29");
}

dtrace:::END
{
	trace("30");
	trace("31");
}
