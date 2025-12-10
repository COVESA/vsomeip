# Reuse Client ID Test

Check if reusing previously assigned clientIDs do not create incompatibilities.

## Purpose

This test aims to ensure that if a clientID becomes available for reassignment again, it is able to be so, even if its previous "owner" re-registers.

The config used in this test (diagnosis_mask) reduces the number of available clientIDs to 4.

## Test Logic

```
Start routing manager

Start App1 (expect 6301)
Start App2 (expect 6302)
Start App3 (expect 6303)
Start App4 (expect not to register, no available clientIDs)

# All should register but App4 (max clients reached)

Stop App4

Stop App1

Start App5 (expect 6301)

Stop App2

Start App1 (starts with 6301) (no available clientIDs)

# App1 should register successfully now (clientID should be different than the 1st one)

Stop App1
Stop App3
Stop App5

Stop routing manager

```
