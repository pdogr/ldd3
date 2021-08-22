#!/usr/bin/env bash
module="scullpa"
device="scullpa"

rmmod $module $* || exit 1
rm -rf /dev/${device} /dev/${device}[0-3]
