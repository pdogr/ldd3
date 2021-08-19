#!/usr/bin/env bash
module="scullp"
device="scullp"

rmmod $module $* || exit 1
rm -rf /dev/${device} /dev/${device}[0-3]
