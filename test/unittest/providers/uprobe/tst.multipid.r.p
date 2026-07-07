#!/usr/bin/gawk -f

# n = PID NNN
$3 == "PID" { $4 = "NNN"; print; }
