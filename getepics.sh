#!/bin/bash
export BACNET_APDU_RETRIES=10
export BACNET_APDU_TIMEOUT=9999
export BACNET_BBMD_PORT=47808
echo trying 10.7.74.11
BACNET_BBMD_ADDRESS=10.7.74.11 ./bin/bacepics 597 -n 10.7.74.0/24 > epics597.log3
echo trying 10.7.74.12
BACNET_BBMD_ADDRESS=10.7.74.12 ./bin/bacepics -d 598 > epics598.log3
echo trying 10.7.74.13
BACNET_BBMD_ADDRESS=10.7.74.13 ./bin/bacepics -d 502 > epics502.log3
echo trying 10.7.74.14
BACNET_BBMD_ADDRESS=10.7.74.14 ./bin/bacepics -d 503 > epics503.log3
echo trying 10.7.74.15
BACNET_BBMD_ADDRESS=10.7.74.15 ./bin/bacepics -d 596 > epics596.log3
echo trying 10.7.74.16
BACNET_BBMD_ADDRESS=10.7.74.16 ./bin/bacepics -d 509 > epics509.log3
echo trying 10.7.74.19
BACNET_BBMD_ADDRESS=10.7.74.19 ./bin/bacepics -d 500 > epics500.log3
echo trying 10.7.74.20
BACNET_BBMD_ADDRESS=10.7.74.20 ./bin/bacepics -d 591 > epics591.log3
echo done
