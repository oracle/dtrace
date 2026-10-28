/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Aggregations are not data-recording.
 *
 * SECTION: Aggregations/Aggregations
 */

/* three default actions */
dtrace:::BEGIN { }
dtrace:::BEGIN { }
dtrace:::BEGIN { }

/* one explicit action */
dtrace:::BEGIN { trace("hello world"); }

/* ten non-data-recording actions */
dtrace:::BEGIN { @a = count(); }
dtrace:::BEGIN { @a = count(); }
dtrace:::BEGIN { @a = count(); }
dtrace:::BEGIN { @a = count(); }
dtrace:::BEGIN { @a = count(); }
dtrace:::BEGIN { @a = count(); }
dtrace:::BEGIN { @a = count(); }
dtrace:::BEGIN { @a = count(); }
dtrace:::BEGIN { @a = count(); }
dtrace:::BEGIN { @a = count(); }

/* exit */
dtrace:::BEGIN { exit(0); }
