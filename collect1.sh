#!/bin/bash
# collects data for the no of page faults for range of NumPhysPages values
# Usage 
#   First argument: name of test file
#   Second argument: lower bound for NumPhysPages
#   Third argument: upper bound for NumPhysPages
for i in `seq $2 $3`;
do
y=nachos/code/test/$1
echo -n $i,
/home/ndesh/Studies/5sem/cs330/OS-Assignments/nachos/code/userprog/nachos -R 2 -rs 0 -A 3 -T $i -x $y | grep fault | cut -d " " -f 3 
done
