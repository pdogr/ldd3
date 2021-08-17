#!/usr/bin/env bash
module="scull"
device="scull"

rmmod $module $* || exit 1
rm -rf /dev/${device} /dev/${device}[0-3]
