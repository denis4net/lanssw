#!/usr/bin/python
from signal import SIGVTALRM

__author__ = 'Denis Vashchuk'

import socket
import struct
import time
import argparse
import sys
import os
import signal
import random

ICMP_ECHO_REQUEST = 8
ICMP_CODE = socket.getprotobyname('icmp')

options = None # arguments namespace

def checksum(data):
    sum = 0
    count_to = (len(data) / 2) * 2
    count = 0
    while count < count_to:
        this_val = (data[count + 1] << 8)+data[count]
        sum += this_val
        sum &= 0xffffffff
        count += 2
    if count_to < len(data):
        sum += data[len(data) - 1]
        sum &= 0xffffffff

    sum = (sum >> 16) + (sum & 0xffff)
    sum += (sum >> 16)
    answer = ~sum
    answer &= 0xffff
    return answer

def create_icmp_packet(id, sequence_number):
    """Create a new echo request packet based on the given id."""
    # Header is type (8), code (8), checksum (16), id (16), sequence (16)
    sent_time = None
    sent_time = time.time()
    packet = struct.pack('!BBHHHd', ICMP_ECHO_REQUEST, 0, 0, id, sequence_number, sent_time)
    # icmp packet data is 32b float that means time of send echo request

    # Calculate the checksum on the data and the dummy header.
    icmp_checksum = checksum(packet)
    # Now that we have the right checksum, we put that in. It's just easier
    # to make up a new header than to stuff it into the dummy.
    packet = struct.pack('!BBHHHd', ICMP_ECHO_REQUEST, 0, socket.htons(icmp_checksum), id, sequence_number, sent_time)

    return packet

def send_icmp_packet(sockfd, dest_addr, packet_id, sequence_number, timeout=1):
    # Generate icmp packet with given icmp packet id
    packet = create_icmp_packet(packet_id, sequence_number)
    while packet:
        # The icmp protocol does not use a port, but the function
        # below expects it, so we just give to it a dummy port.
        sent = sockfd.sendto(packet, (dest_addr, 1))
        packet = packet[sent:]

def receive_icmp_packet(my_socket, packet_id):
    # Receive the ping from the socket.
    while True:
            try:
                rec_packet, addr = my_socket.recvfrom(36)
            except InterruptedError:
                continue

            time_received = time.time()
            icmp_packet = rec_packet[20:]
            type, code, checksum, p_id, sequence, time_sent = struct.unpack('!BBHHHd', icmp_packet)
            # print("time sent %f, time received %f" % (time_sent, time_received))
            # print(''.join( ('%x '% byte) for byte in icmp_packet))
            #if p_id == packet_id and type == 0 and code == 0:
            print("icmp_response from %s, icmp_sequence=%d, time=%fs" % (str(addr[0]), sequence, time_received - time_sent))
            #     return
            #elif p_id == packet_id and type != 8: #echo request
            #    print("icmp_response type=%d, code=%d, icmp_sequence=%d" % (type, code, sequence))
            #    return

def pinger():
    global options
    options.sequence_number += 1
    send_icmp_packet(sockfd=options.sockfd, dest_addr=options.destination,
                     packet_id=options.packet_id, sequence_number=options.sequence_number)

def signanl_handler(signo, frame_obj):
    if signo == signal.SIGALRM:
        pinger()

def parse_options(argv):
    parser = argparse.ArgumentParser(description="Ping arguments")
    parser.add_argument('destination', type=str, metavar='destination', help='set destination address')
    parser.add_argument('-i', type=float, metavar='interval', help='set icmp packet send interval', default=1.0)
    options = parser.parse_args(argv)
    return options

def main():
    global options

    options = parse_options(sys.argv[1:])
    signal.signal(signal.SIGALRM, signanl_handler)

    if os.geteuid() != 0:
        sys.stderr.write("ping uses RAW SOCKETs therefore must running with root privilegies\n")
        sys.exit(-1)

    try:
        sockfd = socket.socket(socket.AF_INET, socket.SOCK_RAW, socket.IPPROTO_ICMP)
        options.sockfd = sockfd
        options.packet_id = os.getpid() & 0xFFFF
        options.destination = socket.gethostbyname(options.destination)
        options.sequence_number = 0

        print("PING %s, interval %d, packed identifier %d" % (options.destination, options.i, options.packet_id))
        signal.setitimer(signal.ITIMER_REAL, options.i, options.i)
        while True:
            receive_icmp_packet(sockfd, options.packet_id)
    except socket.error as e:
        sys.stderr.write("Exception: " + e.strerror + '\n')



if __name__ == '__main__':
    main()


