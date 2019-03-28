#!/bin/sh

#
# Copyright (C) 2018 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

# use setcap tool only when it's available
# First, check AVB_SETCAP_TOOL var
if [ -x "$AVB_SETCAP_TOOL" ]; then
	echo "Using setcap_tool defined on AVB_SETCAP_TOOL env var"
	$AVB_SETCAP_TOOL $1 && exit
	echo "Failed to set cap"
fi
which setcap_tool &> /dev/null
if [ $? -eq 0 ]; then
	echo "Trying to use setcap_tool on PATH env var"
	setcap_tool $1 && exit
	echo "Failed to set cap"
fi
which setcap &> /dev/null
if [ $? -eq 0 ]; then
	echo "running setcap on PATH env var"
	setcap cap_net_admin,cap_net_raw,cap_sys_nice+ep $1 && exit
	echo "Failed to set cap"
fi

echo "Warning: echo setcap_tool not installed"
