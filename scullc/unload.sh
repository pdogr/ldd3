#!/usr/bin/env bash
module="scullc"
device="scullc"

rmmod $module $* || exit 1
rm -rf /dev/${device} /dev/${device}[0-3]
