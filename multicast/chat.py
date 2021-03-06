#!/usr/bin/env python2
# -*- coding: utf-8 -*-
import argparse
import json
import threading
import select
from time import strftime
import socket
import sys
import netifaces

HELP_MESSAGE = """
"""

PORT = 5455
PACKET_SIZE = 1024

DEBUG = False

PUBLIC_ROOM_IP = "224.10.10.10"
PRIVATE_ROOM_IPS = [
    "224.10.10.15",
    "224.10.10.16",
    "224.10.10.17",
    "224.10.10.18",
    "224.10.10.19",
]

class ChatThread(threading.Thread):
    def __init__(self, chat):
        super(ChatThread, self).__init__()
        self._chat = chat
        self._terminated = False
    def run(self):
        while not self._terminated:
            self._chat.process()
    def shutdown(self):
        self._terminated = True

class MulticastChat(object):

    def __init__(self, nick, my_ip):
        self._nick = nick
        self._my_ip = my_ip
        self._rooms = set()
        try:
            self._sockfd = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
            self._sockfd.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self._sockfd.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)

            if DEBUG:
                self._sockfd.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_LOOP, 1)
            else:
                self._sockfd.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_LOOP, 0)

            self._sockfd.setsockopt(socket.SOL_IP, socket.IP_MULTICAST_IF, socket.inet_aton(my_ip))
            self._sockfd.bind(('', PORT))
        except socket.error as e:
            print("Error when creating a socket")
        self.join_room(0)

    def join_room(self, room_number):
        if room_number in self._rooms:
            print("Already a member")
            return
        ip = self.get_room_ip(room_number)
        if ip:
            self.add_membership(ip)
            self.say_hello(room_number)
            self._rooms.add(room_number)
            if room_number == 0:
                print("joined public room")
            else:
                print("joined room %d" % room_number)


    def leave_room(self, room_number):
        if not room_number in self._rooms:
            print("You are not member of this room")
            return
        ip = self.get_room_ip(room_number)
        if ip:
            self.say_goodbye(room_number)
            self.drop_membership(ip)
            self._rooms.remove(room_number)
            if room_number == 0:
                print("leaved public room")
            else:
                print("leaved room %d" % room_number)


    def list_rooms(self):
        if not self._rooms:
            return
        print("You are member of rooms:")
        for room in self._rooms:
            if room == 0:
                print("Public room")
            else:
                print("Room %d" % room)

    def get_room_ip(self, room_number):
        try:
            room_number = int(room_number)
        except ValueError as e:
            print("Invalid room number")
            return
        if room_number == 0:
            return PUBLIC_ROOM_IP
        elif 0 < room_number < 5:
            return PRIVATE_ROOM_IPS[room_number-1]

    def get_nick(self):
        return self._nick


    def say_hello(self, room_number):
        ip = self.get_room_ip(room_number)
        if ip:
            data = {
            'nick': self._nick,
            'command': 'hello',
            'room': room_number
            }
            enc_data = json.dumps(data)
            self._sockfd.sendto(enc_data, (self.get_room_ip(room_number), PORT))

    def say_goodbye(self, room_number):
        ip = self.get_room_ip(room_number)

        if ip:
            data = {
            'nick': self._nick,
            'command': 'goodbye',
            'room': room_number
            }
            enc_data = json.dumps(data)
            self._sockfd.sendto(enc_data, (self.get_room_ip(room_number), PORT))


    def add_membership(self, ip):
        self._sockfd.setsockopt(socket.IPPROTO_IP,
                                 socket.IP_ADD_MEMBERSHIP,
                                 socket.inet_aton(ip) +
                                 socket.inet_aton(self._my_ip))

    def drop_membership(self, ip):
        self._sockfd.setsockopt(socket.IPPROTO_IP,
                                 socket.IP_DROP_MEMBERSHIP,
                                 socket.inet_aton(ip) +
                                 socket.inet_aton(self._my_ip))

    def process(self):
        r, _, _ = select.select([self._sockfd, ], [], [], 0.5)
        if r:
            packet, addr = self._sockfd.recvfrom(PACKET_SIZE)
            if (addr[0] == self._my_ip) and (addr[1] == PORT):
                #don't receive packets from yourself
                if not DEBUG:
                    return
                print("my packet\n")
            try:
                data = json.loads(packet)
                if "message" in data:
                    if "nick" in data:
                        print("Received message '%s' from user '%s' at %s, room %d" % (data['message'], data['nick'], strftime("%Y-%m-%d %H:%M:%S"), data['room']))
                elif "command" in data:
                    self.parse_command(addr, data['command'], data)
            except ValueError as e:
                print("invalid packet received: ", e.strerror)

    def send_nick(self, address):
        data = {
            'nick': self._nick,
            'command': 'pong'
        }
        enc_data = json.dumps(data)
        self._sockfd.sendto(enc_data, address)

    def send_room(self, message, room_number=0):
        data = {
            "nick": self._nick,
            "message": message,
            'room': room_number
        }
        data_enc = json.dumps(data)
        if room_number not in self._rooms:
            print("Not a member of room %s" % room_number)
            return
        ip = self.get_room_ip(room_number)
        if ip:
            self._sockfd.sendto(data_enc, (ip, PORT))

    def list(self, room_number=0):
        """send command to list chat users"""
        data = {
            "command": "ping",
            "room": room_number
        }
        data_enc = json.dumps(data)
        ip = self.get_room_ip(room_number)
        if ip:
            self._sockfd.sendto(data_enc, (ip, PORT))

    def parse_command(self, address, command, data):
        if command == "ping":
            self.send_nick(address)
        elif command == "pong":
            if 'nick' in data:
                print("\nUser '%s' ip '%s'" % (data['nick'], address[0]))
        elif command == "hello":
            print("\nUser '%s' joined room %d" % (data['nick'],data['room']))
        elif command == "goodbye":
            print("\nUser '%s' leaved room %d" % (data['nick'],data['room']))

    def close(self):
        for room in list(self._rooms):
            self.leave_room(room)
        self._sockfd.close()


def print_protompt():
    sys.stdout.write("> ")

def main():
    parser = argparse.ArgumentParser(description='Python broadcast chat')
    parser.add_argument("nickname", help="User nickname in chat")
    parser.add_argument("interface", help="Network interface")
    args = parser.parse_args()
    iface_info = netifaces.ifaddresses(args.interface)

    if iface_info[2] is None:
        sys.stderr.write("Can't get IPv4 address info\n")
        return False

    host_addr = iface_info[2][0]["addr"]

    print("******Interface params******")
    print("host_addr ", host_addr)
    chat = MulticastChat(args.nickname, host_addr)
    thread = ChatThread(chat)
    thread.start()
    try:
        while True:
            print_protompt()
            command = sys.stdin.readline()[:-1]
            command_list = command.split()
            if not command:
                continue
            elif command in ["/help", "!h"]:
                print(HELP_MESSAGE)
            elif command in ["/quit", "!q"]:
                break
            elif command in ["/list", "!l"]:
                print("User lists:")
                chat.list()
            elif command in ('/whoami', '!i'):
                print("Your nick is %s" % chat.get_nick())
            elif (len(command_list) == 2) and command_list[0] in ("/list", "!l"):
                try:
                    room = int(command_list[1])
                except ValueError as e:
                    print("Invalid room value")
                    continue
                print("Room %d users list" % room)
                chat.list(room)
            elif command in ["/rooms", "!r"]:
                chat.list_rooms()
            elif (len(command_list) == 2) and (command_list[0] in ('/join', '!j')):
                try:
                    room = int(command_list[1])
                except ValueError as e:
                    print("Invalid room number")
                    continue
                chat.join_room(room)
            elif (len(command_list) == 2) and (command_list[0] in ('/leave', '!lv')):
                try:
                    room = int(command_list[1])
                except ValueError as e:
                    print("Invalid room number")
                    continue
                chat.leave_room(room)
            elif (len(command_list) > 1) and (command_list[0] in ("/send",  "!s")):
                message = ' '.join(command_list[1:])
                chat.send_room(message)
            elif (len(command_list) > 2) and (command_list[0] in ("/sendroom",  "!sr")):
                try:
                    room = int(command_list[1])
                except ValueError as e:
                    print("Invalid room number")
                    continue
                print()
                message = ' '.join(command_list[2:])
                chat.send_room(message, room)
            else:
                print("Invalid command. type '/help' or '!h' for help")
    except KeyboardInterrupt:
        print("\nGood bye!")
    finally:
        thread.shutdown()
        thread.join()
        chat.close()



if __name__ == "__main__":
    main()
