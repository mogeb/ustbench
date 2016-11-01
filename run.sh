#!/bin/bash

set -x

lttng create --snapshot -o .
lttng enable-channel chan0 --subbuf-size 8k --num-subbuf 2 -u
lttng enable-event -u -c chan0 ustbench:bench_tp_4b
lttng start 
./bin/ustbench -n 15000 -p 1 -t lttng-ust
lttng stop

