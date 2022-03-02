#! /bin/bash

self_pid=$$

echo "test.sh"
echo "pid = ${self_pid}"

function self_kill() {
    kill -s 2 ${self_pid}
}

function onSelfKill () {
    echo "onSelfKill"
    exit 3
}


# trap 'onSelfKill' 1

ls -l ./
sleep 20s

echo "call 2.sh"
source ./2.sh
echo "end call 2.sh"

# self_kill
ret=$?
echo "ret = ${ret}"
if [ ${ret} -ne 0 ]; then
    exit ${ret}
fi


exit 1

