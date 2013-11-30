#!/bin/sh
if [ ! -f  out/client ]; then
	make client
fi

for((i=0;i<5;i++)); do
out/client -a 127.0.0.1 -p 1234 -s /home/denis/Images/BT5R3-KDE-64/BT5R3-KDE-64.iso -d /home/denis/tmp/bt${i}.iso &
done
