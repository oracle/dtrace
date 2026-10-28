/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 *
 * ASSERTION:
 * Multiple flags after a -- should not crash.
 *
 */

/* @@runtest-opts: -- -e -e */

dtrace:::BEGIN { exit(0); }
