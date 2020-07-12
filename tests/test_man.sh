#!/bin/bash
#
# A simple shell script to validate that all client functions listed
# in client.c are found inside of the man pages

client_functions=$(grep -E "{.." ../client.c | cut -d" " -f6 | sed -r 's/"//g' | sed -r 's/,//g' | sort)
man_functions=$(grep -E "\\fB" ../berryc.1 | sed -r 's/\\fB//g' | sed -r 's/(\\fR.*)//g' | sed -r 's/\[//g' | sed -r 's/\]//g' | tr "|" '\n' | sort)
echo "MISSING FUNCTIONS"
comm -23 <(echo "$client_functions") <(echo "$man_functions")
