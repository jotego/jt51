#!/bin/bash

echo "This test runs DD2, Willow and Ghouls first seconds, which seem to"
echo "exercise the LFO considerably"

sim.sh -f dd2.vgz -time 3000
sim.sh -f willow.vgm -time 3000 -runonly&
sim.sh -f ghouls.vgz -time 13000 -runonly&
sim.sh -f wagon.vgz  -time 13000 -runonly&

wait
