/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* limit the string size to relieve pressure on the BPF verifier */
#pragma D option quiet
#pragma D option strsize=184

dtrace:::BEGIN
{
	str = "foobarbarbazbarbop";
	this->success = 0;

	c = str;
	printf("strstr(\"%s\", \"%s\") = \"%s\"\n", str, c, strstr(str, c));

	c = "baz";
	printf("strstr(\"%s\", \"%s\") = \"%s\"\n", str, c, strstr(str, c));

	c = "bar";
	printf("strstr(\"%s\", \"%s\") = \"%s\"\n", str, c, strstr(str, c));

	c = "bazbarbop";
	printf("strstr(\"%s\", \"%s\") = \"%s\"\n", str, c, strstr(str, c));

	c = "barba";
	printf("strstr(\"%s\", \"%s\") = \"%s\"\n", str, c, strstr(str, c));

	printf("strstr(\"%s\", \"%s\") = \"%s\"\n",
	    strstr(str, "baz"), strstr(str, "zba"),
	    strstr(strstr(str, "baz"), strstr(str, "zba")));

	c = "";
	printf("strstr(\"%s\", \"%s\") = \"%s\"\n", str, c, strstr(str, c));

	str = "";
	printf("strstr(\"%s\", \"%s\") = \"%s\"\n", str, c, strstr(str, c));

	str = "f";
	c = "f";
	printf("strstr(\"%s\", \"%s\") = \"%s\"\n", str, c, strstr(str, c));

	this->success = 1;
}

dtrace:::BEGIN
/!this->success/
{
	exit(1);
}

dtrace:::BEGIN
/strstr(str, "barbarf") != NULL/
{
	exit(2);
}

dtrace:::BEGIN
/strstr(str, "foobarbarbazbarbopp") != NULL/
{
	exit(3);
}

dtrace:::BEGIN
{
	exit(0);
}
