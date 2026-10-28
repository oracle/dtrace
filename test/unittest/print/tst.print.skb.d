/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@nosort */

#pragma D option quiet

dtrace:::BEGIN
{
        skb = (struct sk_buff *)alloca(sizeof (struct sk_buff));
        skb->len = 123;
        skb->data = (unsigned char *)0xfeedfacefeedface;
        skb->truesize = 32;
        print(skb);
        exit(0);
}
