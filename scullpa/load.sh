#!/usr/bin/env bash
module="scullpa"
device="scullpa"
mode="644"

/sbin/insmod ./$module.ko $* || exit 1
major=$(awk "\$2==\"$module\" {print \$1}" /proc/devices)

if test -z "$major"
then
 echo "not implemented"
 exit 1
fi

rm -f /dev/${device}[0-3]
mknod /dev/${device}0 c $major 0
mknod /dev/${device}1 c $major 1
mknod /dev/${device}2 c $major 2
mknod /dev/${device}3 c $major 3

group="staff"

grep -q '^staff:' /etc/group || group="wheel"

chgrp $group /dev/${device}[0-3]
chmod $mode /dev/${device}[0-3]




