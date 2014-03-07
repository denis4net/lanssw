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
import ip
import inetutils

ICMP_ECHO_REQUEST = 8
options = None # arguments namespace

def create_ip_packet(src, dst, icmp_id, sequence_number):
    """Create a new echo request packet based on the given id."""
    # Header is type (8), code (8), checksum (16), id (16), sequence (16)
    sent_time = time.time()
    icmp = struct.pack('!BBHHHd', ICMP_ECHO_REQUEST, 0, 0, icmp_id, sequence_number, sent_time)
    # inetutils.cksum return unsigned short in network bytes order
    icmp_checksum = inetutils.cksum(icmp)
    # therefore we must convert it to host endianess than convert all icmp header to bytes array in network endianess
    icmp = struct.pack('!BBHHHd', ICMP_ECHO_REQUEST, 0, socket.ntohs(icmp_checksum), icmp_id, sequence_number, sent_time)
    # Build ip packet
    # Give ipv4 destination and source address and data (icmp packet) as arguments
    packet = ip.Packet(src=src, dst=dst, ttl=64, p=socket.IPPROTO_ICMP, data=icmp)

    return packet._assemble(cksum=True)

def send_ip_packet(sockfd, dest_addr, source_addr, icmp_id, sequence_number, timeout=1):
    # Generate icmp packet with given icmp packet id
    packet = create_ip_packet(src=source_addr, dst=dest_addr, icmp_id=icmp_id, sequence_number=sequence_number)
    while packet:
        # The icmp protocol does not use a port, but the function
        # below expects it, so we just give to it a dummy port.
        sent = sockfd.sendto(packet, (options.destination, 0))
        packet = packet[sent:]

def receive_ip_packet(my_socket, packet_id):
    # Receive the ping from the socket.
    while True:
            try:
                rec_packet, addr = my_socket.recvfrom(36)
            except InterruptedError:
                continue

            time_received = time.time()
            icmp_packet = rec_packet[20:]
            type, code, checksum, p_id, sequence, time_sent = struct.unpack('!BBHHHd', icmp_packet)

            print(''.join( ('%x '% byte) for byte in icmp_packet))

            if p_id == packet_id and type == 0 and code == 0:
                print("icmp_response from %s, icmp_sequence=%d, time=%fs" % (str(addr[0]), sequence, time_received - time_sent))
                return
            elif p_id == packet_id and type != 8: #echo request
                print("icmp_response type=%d, code=%d, icmp_sequence=%d" % (type, code, sequence))
                return

def pinger():
    global options
    options.sequence_number += 1
    src_addr = socket.gethostbyname(options.s)
    send_ip_packet(sockfd=options.sockfd, dest_addr=options.destination, source_addr=src_addr,
                     icmp_id=options.packet_id, sequence_number=options.sequence_number)

def signanl_handler(signo, frame_obj):
    if signo == signal.SIGALRM:
        pinger()

def parse_options(argv):
    parser = argparse.ArgumentParser(description="Ping arguments")
    parser.add_argument('destination', type=str, metavar='destination', help='set destination address')
    parser.add_argument('-c', type=int, metavar='count', help='retries count')
    parser.add_argument('-s', type=str, metavar='source', help='set source address', default='0.0.0.0')
    parser.add_argument('-i', type=float, metavar='interval', help='set icmp packet send interval', default=1.0)
    parser.add_argument('-t', type=float, metavar='timeout', help='icmp echo response wait timeout', default=3.0)
    options = parser.parse_args(argv)
    return options

def main():
    global options
    if os.geteuid() != 0:
        sys.stderr.write("ping uses RAW SOCKETs therefore must running with root privilegies\n")
        sys.exit(-1)

    options = parse_options(sys.argv[1:])
    signal.signal(signal.SIGALRM, signanl_handler)

    try:
        sockfd = socket.socket(socket.AF_INET, socket.SOCK_RAW, socket.IPPROTO_RAW)
        sockfd.setsockopt(socket.IPPROTO_IP, socket.IP_HDRINCL, True)

        sockfd_r = socket.socket(socket.AF_INET, socket.SOCK_RAW, socket.IPPROTO_ICMP)

        options.sockfd = sockfd
        options.packet_id = os.getpid() % 0xFFFFFF
        options.destination = socket.gethostbyname(options.destination)
        options.sequence_number = 0

        print("PING %s, interval %d, packed identifier %d" % (options.destination, options.i, options.packet_id))
        signal.setitimer(signal.ITIMER_REAL, options.i, options.i)
        while True:
            receive_ip_packet(sockfd_r, options.packet_id)

    except socket.error as e:
        sys.stderr.write("Exception: " + e.strerror + '\n')

if __name__ == '__main__':
    main()


