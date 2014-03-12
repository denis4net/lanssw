#!/usr/bin/env python2
# -*- coding: utf-8 -*-
import argparse
import json
import threading
import select
from time import strftime
import netifaces
import sys
import socket


PORT = 5455
PACKET_SIZE = 1024

HELP_MESSAGE = """/help - this help message
/list - list chat users
/quit - quit a chat
/send <message> - send a message"""

class ChatThread(threading.Thread):
    def __init__(self, chat):
        super(ChatThread, self).__init__()
        self._chat = chat
        self._terminated = False

    def run(self):
        while True:
            self._chat.process()
            if self._terminated:
                break
    def shutdown(self):
        self._terminated = True

class Chat(object):
    def __init__(self, nick, ipaddr, bcast):
        self._nick = nick
        self._bcast = bcast
        self._my_ip = ipaddr
        try:
            self._sockfd = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
            self._sockfd.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self._sockfd.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
            self._sockfd.bind(('', PORT))
        except socket.error as e:
            sys.stderr.write(e.strerror)

    def process(self):
        r, _, _ = select.select([self._sockfd], [], [], 0.5)
        if r:
            packet, addr = self._sockfd.recvfrom(PACKET_SIZE)
            # skip packet from youself
            if (addr[0] == self._my_ip) and (addr[1] == PORT):
                return

            try:
                data = json.loads(packet)
                if "message" in data:
                    if "nick" in data:
                        print("Received message '%s' from user '%s' at %s" % (data['message'], data['nick'], strftime("%Y-%m-%d %H:%M:%S")))
                elif "command" in data:
                    self.parse_command(addr, data['command'], data)
            except ValueError as e:
                print("invalid packet received")

    def send_nick(self, address):
        data = {
            'nick': self._nick,
            'command': 'pong'
        }
        enc_data = json.dumps(data)
        self._sockfd.sendto(enc_data, address)

    def send(self, message):
        data = {
            "nick": self._nick,
            "message": message
        }
        data_enc = json.dumps(data)
        self._sockfd.sendto(data_enc, (self._bcast, PORT))

    def list(self):
        """send command for getting chat users list"""
        data = {"command": "ping"}
        data_enc = json.dumps(data)
        self._sockfd.sendto(data_enc, (self._bcast, PORT))

    def parse_command(self, address, command, data):
        if command == "ping":
            self.send_nick(address)
        elif command == "pong":
            if 'nick' in data:
                print("\nUser '%s' ip '%s'" % (data['nick'], address[0]))

    def close(self):
        self._sockfd.close()

def print_protompt():
    sys.stdout.write("> ")

def chat_cli():
    parser = argparse.ArgumentParser(description='Python broadcast chat')
    parser.add_argument("nickname", help="User nickname in chat")
    parser.add_argument("interface", help="Network interface")

    args = parser.parse_args()

    ifname = args.interface
    iface_info = netifaces.ifaddresses(ifname)

    host_addr = iface_info[2][0]["addr"]
    netmask = iface_info[2][0]["netmask"]
    bcast_addr = iface_info[2][0]["broadcast"]

    print("******Interface params******")
    print("host_addr ", host_addr)
    print("netmask   ", netmask)
    print("bcast_addr", bcast_addr)

    chat = Chat(args.nickname, host_addr, bcast_addr)
    thread = ChatThread(chat)
    thread.start()

    try:
        while True:
            print_protompt()
            command = sys.stdin.readline()[:-1]
            if command in ["/help", "!h"]:
                print(HELP_MESSAGE)
            elif command in ("/list", "!l"):
                print("User lists:")
                chat.list()
            elif (command[:5] == '/send') or (command[:2] == r'!s'):
                if command.startswith("/send "):
                    message = command[6:]
                elif command.startswith("!s"):
                    message = command[3:]
                else:
                    print("Incorrect command")
                    continue
                chat.send(message)
            elif not command:
                continue
            else:
                print("Invalid command. type '/help' or '!h' for help message")
    except KeyboardInterrupt:
        print("\nInterrupted by keyboard. Good bye!")
    finally:
        thread.shutdown()
        thread.join()
        chat.close()

if __name__ == "__main__":
    chat_cli()
