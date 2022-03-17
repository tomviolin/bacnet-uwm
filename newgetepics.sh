#!/bin/bash
export BACNET_APDU_RETRIES=10
export BACNET_APDU_TIMEOUT=9999
export BACNET_BBMD_PORT=47808
#BACNET_BBMD_ADDRESS=10.7.74.11 ./bin/bacepics 597 -n 10.7.74.0/24 > epics597.log3

BACNET_BBMD_ADDRESS=10.7.74.11 ./bin/bacepics 597 -t 0A:07:4A:0B:BA:C0 > epics597.txt2
BACNET_BBMD_ADDRESS=10.7.74.12 ./bin/bacepics 598 -t 0A:07:4A:0C:BA:C0 > epics598.txt2
BACNET_BBMD_ADDRESS=10.7.74.13 ./bin/bacepics 595 -t 0A:07:4A:0D:BA:C0 > epics595.txt2
BACNET_BBMD_ADDRESS=10.7.74.14 ./bin/bacepics 621 -t 0A:07:4A:0E:BA:C0 > epics621.txt2
BACNET_BBMD_ADDRESS=10.7.74.15 ./bin/bacepics 596 -t 0A:07:4A:0F:BA:C0 > epics596.txt2
BACNET_BBMD_ADDRESS=10.7.74.16 ./bin/bacepics 507 -t 0A:07:4A:10:BA:C0 > epics507.txt2
BACNET_BBMD_ADDRESS=10.7.74.19 ./bin/bacepics 579 -t 0A:07:4A:13:BA:C0 > epics579.txt2
BACNET_BBMD_ADDRESS=10.7.74.20 ./bin/bacepics 591 -t 0A:07:4A:14:BA:C0 > epics591.txt2

