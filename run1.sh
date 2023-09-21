#!/bin/bash

clear

rm output.png

make clean

make

./okwarp west.jpeg output.png
