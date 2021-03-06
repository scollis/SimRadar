#!/bin/bash

function show_last_seg() {
	cmd="grep -n \"==================<<<\" \"$1\" | tail -n 1 | awk -F \":\" '{print \$1}'"
	line=`eval $cmd`
        if [ ! -z ${line} ]; then
		tail -n +$line $1
	fi
}


if [[ "$#" -gt 0 && "$1" == "t" ]]; then
	# Test
	echo -e "\033[33m"
	show_last_seg tests_stdout.txt
	echo -e "\033[31m"
	show_last_seg tests_stderr.txt
elif [[ "$#" -gt 0 && "$1" == "1" ]]; then
	# Single node execution
	echo -e "\033[32m"
	show_last_seg radarsim_single_stdout.txt
	echo -e "\033[31m"
	show_last_seg radarsim_single_stderr.txt
elif [[ "$#" -gt 0 && "$1" == "c" ]]; then
	echo -e "\033[36m"
	show_last_seg radarsim_cpu.txt
else
	echo -e "\033[32m"
	show_last_seg radarsim_stdout.txt
	echo -e "\033[31m"
	show_last_seg radarsim_stderr.txt
fi

echo -e "\033[0m"

