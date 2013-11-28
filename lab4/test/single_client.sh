#!/bin/sh
if [ ! -f  out/client ]; then
	make client
fi

out/client 127.0.0.1 1234 /home/denis/Images/BT5R3-KDE-64/BT5R3-KDE-64.iso /home/denis/tmp/bt1.iso

