#!/usr/bin/gawk -f

# 1st CPU is %d
/^1st/ { cpu_a = $NF; $NF = "A"; print; next; }
# 2nd CPU is %d
/^2nd/ { cpu_b = $NF; $NF = "B"; print; next; }
# cpu %d
/^cpu/ { $NF = ($NF == cpu_a) ? "A" : ($NF == cpu_b) ? "B" : "X"; print; }
