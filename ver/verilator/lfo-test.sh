#!/bin/bash

echo "This test runs DD2, Willow and Ghouls first seconds, which seem to"
echo "exercise the LFO considerably"

go -f dd2.vgz -time 3000
go -f willow.vgm -time 3000 -runonly&
go -f ghouls.vgz -time 13000 -runonly&
go -f wagon.vgz  -time 13000 -runonly&

wait
