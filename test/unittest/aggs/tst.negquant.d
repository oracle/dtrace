/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

dtrace:::BEGIN
{
	@["j-church"] = quantize(1, 100);
	@["j-church"] = quantize(1, -99);
	@["j-church"] = quantize(1, -1);
	val = 123;
}

dtrace:::BEGIN
{
	@["k-ingleside"] = quantize(1, -val);
}

dtrace:::BEGIN
{
	@["l-taraval"] = quantize(0, -val);
	@["l-taraval"] = quantize(-1, -val);
	@["l-taraval"] = quantize(1, val);
	@["l-taraval"] = quantize(1, val);
}

dtrace:::BEGIN
{
	@["m-oceanview"] = quantize(1, (1 << 63) - 1);
	@["m-oceanview"] = quantize(1);
	@["m-oceanview"] = quantize(2, (1 << 63) - 1);
	@["m-oceanview"] = quantize(8, 400000);
}

dtrace:::BEGIN
{
	@["n-judah"] = quantize(1, val);
	@["n-judah"] = quantize(2, val);
	@["n-judah"] = quantize(2, val);
	@["n-judah"] = quantize(2);
}

this long long val;

dtrace:::BEGIN
{
	this->i = 1;
	this->val = (((long long) 1) << 63) - 1;

	@["f-market"] = quantize(this->i, this->val);
	this->i <<= 1;
	this->val >>= ((long long) 1);

	@["f-market"] = quantize(this->i, this->val);
	this->i <<= 1;
	this->val >>= ((long long) 1);

	@["f-market"] = quantize(this->i, this->val);
	this->i <<= 1;
	this->val >>= ((long long) 1);

	@["f-market"] = quantize(this->i, this->val);
	this->i <<= 1;
	this->val >>= ((long long) 1);
}

dtrace:::BEGIN
{
	this->i = 1;
	this->val = (((long long) 1) << 63) - 4;

	@["s-castro"] = quantize(this->i, this->val);
	this->i <<= 1;
	this->val >>= ((long long) 1);

	@["s-castro"] = quantize(this->i, this->val);
	this->i <<= 1;
	this->val >>= ((long long) 1);

	@["s-castro"] = quantize(this->i, this->val);
	this->i <<= 1;
	this->val >>= ((long long) 1);

	@["s-castro"] = quantize(this->i, this->val);
	this->i <<= 1;
	this->val >>= ((long long) 1);
}

dtrace:::BEGIN
{
	exit(0);
}
