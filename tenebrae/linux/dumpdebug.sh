#!/bin/sh

gdb << EOF > debug.log 2>&1

file ./debugi386.glibc/bin/tenebrae.run
run -window
where full
kill
quit

EOF

