/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

dtrace:::BEGIN
{
	lv = "Live";
	dt = ", DTrace, ";
	ps = "Prosper";
}

dtrace:::BEGIN
{
	phrase = strjoin(lv, dt);
	phrase = strjoin(phrase, lv);
	expected = "Live, DTrace, Live";
}

dtrace:::BEGIN
/phrase != expected/
{
	printf("global: expected '%s', found '%s'\n", expected, phrase);
	exit(1);
}

dtrace:::BEGIN
{
	this->phrase = strjoin(ps, dt);
}

dtrace:::BEGIN
{
	this->phrase = strjoin(this->phrase, ps);
	expected = "Prosper, DTrace, Prosper";
}

dtrace:::BEGIN
/this->phrase != expected/
{
	printf("clause-local: expected '%s', found '%s'\n",
	    expected, this->phrase);
	exit(2);
}

dtrace:::BEGIN
{
	phrase = NULL;
	this->phrase = NULL;
}

dtrace:::BEGIN
/phrase != NULL/
{
	printf("expected global to be NULL\n");
	exit(3);
}

dtrace:::BEGIN
/this->phrase != NULL/
{
	printf("expected clause-local to be NULL\n");
	exit(4);
}

dtrace:::BEGIN
{
	exit(0);
}
