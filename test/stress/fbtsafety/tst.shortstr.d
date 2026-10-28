/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet
#pragma D option strsize=16

dtrace:::BEGIN
{
	this->str = ",,,Carrots,,Barley,Oatmeal,,,,,,,,,,,,,,,,,,Beans,";
}

dtrace:::BEGIN
{
	strtok(this->str, ",");
}

dtrace:::BEGIN
{
	this->str = ",,,,,,,,,,,,,,,,,,,,,,Carrots,";
	strtok(this->str, ",");
}

dtrace:::BEGIN
{
	strtok(this->str, "a");
}

dtrace:::BEGIN
{
	printf("%s\n", substr(this->str, 1, 40));
}

dtrace:::BEGIN
{
	printf("%s\n", strjoin(this->str, this->str));
}

dtrace:::BEGIN
{
	this->str1 = ".........................................";
	printf("%d\n", index(this->str, this->str1));
}

dtrace:::BEGIN
{
	printf("%d\n", rindex(this->str, this->str1));
}

dtrace:::BEGIN
{
	exit(0);
}
