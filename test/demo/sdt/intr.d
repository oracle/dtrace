/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@xfail: not yet ported */

sdt:::interrupt-start
{
	self->ts = vtimestamp;
}

sdt:::interrupt-complete
/self->ts/
{
	this->devi = (struct dev_info *)arg0;
	@[stringof(`devnamesp[this->devi->devi_major].dn_name),
	    this->devi->devi_instance] = quantize(vtimestamp - self->ts);
}
