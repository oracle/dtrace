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
	trace("00");
	trace("01");
}

dtrace:::BEGIN
{
	trace("02");
	trace("03");
}

dtrace:::BEGIN
{
	trace("04");
	trace("05");
}

dtrace:::BEGIN
{
	trace("06");
	trace("07");
}

dtrace:::BEGIN
{
	trace("08");
	trace("09");
}

dtrace:::BEGIN
{
	trace("10");
	trace("11");
}

dtrace:::BEGIN
{
	trace("12");
	trace("13");
}

dtrace:::BEGIN
{
	trace("14");
	trace("15");
}

dtrace:::BEGIN
{
	trace("16");
	trace("17");
}

dtrace:::BEGIN
{
	trace("18");
	trace("19");
}

dtrace:::BEGIN
{
	trace("20");
	trace("21");
}

dtrace:::BEGIN
{
	trace("22");
	trace("23");
}

dtrace:::BEGIN
{
	trace("24");
	trace("25");
}

dtrace:::BEGIN
{
	trace("26");
	trace("27");
}

dtrace:::BEGIN
{
	trace("28");
	trace("29");
}

dtrace:::BEGIN
{
	trace("30");
	trace("31");
}

dtrace:::BEGIN
{
	exit(0);
}
