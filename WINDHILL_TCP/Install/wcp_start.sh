#! /bin/bash

wcp_name=$1
softlink_wcp="/usr/bin/""${wcp_name}"

if pidof "${wcp_name}"; then
    exit 3
fi

if [ -x "${softlink_wcp}" ]; then
    echo "start \"${softlink_wcp}\"."
    ${softlink_wcp} > /dev/null 2>&1 &
else
    exit 1
fi

