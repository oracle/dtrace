/*
 * Oracle Linux DTrace.
 * Copyright (c) 2007, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@runtest-opts: -C */

#include <stdint.h>
#include <dtrace/dif_defines.h>

#define INTFUNC(x)			\
	dtrace:::BEGIN				\
	/*DSTYLED*/			\
	{				\
		this->ival = (long)(x);	\
		subr++;			\
	/*DSTYLED*/			\
	}

#define AINTFUNC(x)			\
	dtrace:::BEGIN				\
	/*DSTYLED*/			\
	{				\
		subr++;			\
		this->aval = (long)(x);	\
	/*DSTYLED*/			\
	}

#define STRFUNC(x)			\
	dtrace:::BEGIN				\
	/*DSTYLED*/			\
	{				\
		subr++;			\
		this->sval = (x);	\
	/*DSTYLED*/			\
	}

#define VOIDFUNC(x)			\
	dtrace:::BEGIN				\
	/*DSTYLED*/			\
	{				\
		subr++;			\
	/*DSTYLED*/			\
	}

#define STKFUNC(x)			\
	dtrace:::BEGIN				\
	/*DSTYLED*/			\
	{				\
		subr++;			\
		@stk[x] = sum(1);	\
	/*DSTYLED*/			\
	}

#define NUM_UNIMPLEMENTED 7

INTFUNC(rand())
INTFUNC(mutex_owned((struct mutex *)&`bpf_verifier_lock))
INTFUNC(mutex_owner((struct mutex *)&`bpf_verifier_lock))
INTFUNC(mutex_type_adaptive((struct mutex *)&`bpf_verifier_lock))
INTFUNC(mutex_type_spin((struct mutex *)&`bpf_verifier_lock))
INTFUNC(rw_read_held((rwlock_t *)&`tasklist_lock))
INTFUNC(rw_write_held((rwlock_t *)&`tasklist_lock))
INTFUNC(rw_iswriter((rwlock_t *)&`tasklist_lock))
AINTFUNC(copyin(0, 1))
STRFUNC(copyinstr(0, 1))
INTFUNC(speculation())
INTFUNC(progenyof($pid))
INTFUNC(strlen("fooey"))
VOIDFUNC(copyout)
VOIDFUNC(copyoutstr)
AINTFUNC(alloca(10))
VOIDFUNC(bcopy)
VOIDFUNC(copyinto)
/* Not implemented.
   INTFUNC(msgdsize(NULL))
   INTFUNC(msgsize(NULL)) */
INTFUNC(getmajor(0))
INTFUNC(getminor(0))
/* Not implemented.
   STRFUNC(ddi_pathname(NULL, 0)) */
STRFUNC(strjoin("foo", "bar"))
STRFUNC(lltostr(12373))
STRFUNC(basename("/var/crash/systemtap"))
STRFUNC(dirname("/var/crash/systemtap"))
/* Not implemented yet.
   STRFUNC(cleanpath("/var/crash/systemtap")) */
STRFUNC(strchr("The SystemTap, The.", 't'))
STRFUNC(strrchr("The SystemTap, The.", 't'))
STRFUNC(strstr("The SystemTap, The.", "The"))
STRFUNC(strtok("The SystemTap, The.", "T"))
STRFUNC(substr("The SystemTap, The.", 0))
INTFUNC(index("The SystemTap, The.", "The"))
INTFUNC(rindex("The SystemTap, The.", "The"))
INTFUNC(htons(0x1234))
INTFUNC(htonl(0x12345678))
INTFUNC(htonll(0x1234567890abcdefL))
INTFUNC(ntohs(0x1234))
INTFUNC(ntohl(0x12345678))
INTFUNC(ntohll(0x1234567890abcdefL))
/* Not implemented yet.
   STRFUNC(inet_ntop(AF_INET, (void *)alloca(sizeof(ipaddr_t)))) */
STRFUNC(inet_ntoa((ipaddr_t *)alloca(sizeof(ipaddr_t))))
STRFUNC(inet_ntoa6((in6_addr_t *)alloca(sizeof(in6_addr_t))))
/* Not implemented yet.
   STRFUNC(d_path(&(curthread->fs->root)))
   STRFUNC(link_ntop(ARPHRD_ETHER, (void *)alloca(sizeof(ipaddr_t)))) */
STKFUNC(stack(5))
STKFUNC(ustack(5))

dtrace:::BEGIN
/subr == DIF_SUBR_MAX + 1 - NUM_UNIMPLEMENTED/
{
	exit(0);
}

dtrace:::BEGIN
{
	printf("found %d subroutines, expected %d\n", subr, DIF_SUBR_MAX + 1 - NUM_UNIMPLEMENTED);
	exit(1);
}
