#!/bin/bash
if [ ! -f  out/client ]; then
	make client
fi

SOURCE_FILE="/home/denis/tmp/test2.bin"

for((i=0;i<5;i++)); do
	out/client -a 127.0.0.1 -p 1234  -s "$SOURCE_FILE" -d /home/denis/tmp/test-${i}.bin $@ > /dev/null &
done

echo "Waiting for all clients..."

while ( pidof client ); do
	sleep 1s
done

echo "Sending files done"

echo "Source file md5sum"
md5sum "$SOURCE_FILE"

echo "Destination files md5sum"
for((i=0;i<5;i++));do
	md5sum /home/denis/tmp/test-${i}.bin
done


