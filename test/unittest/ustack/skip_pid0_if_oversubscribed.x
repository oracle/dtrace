#!/bin/sh

$dtrace -qn 'profile:::profile-100ms /pid == 0/ { exit(0) }
             profile:::tick-1s { trace("cannot profile pid 0; oversubscribed system?"); exit(2) }'
exit $?
