#!/bin/bash
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
# ASSERTION: Malformed DOF and parser replies are rejected; fragmented replies,
#            stream boundaries, EOF, and stalled parser reads are handled.
test/triggers/usdt-parser
