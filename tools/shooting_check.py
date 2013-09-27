#!/usr/bin/env python


import sys
import elliptics
import logging
import logging.handlers
import re

log = logging.getLogger()
log.setLevel(logging.DEBUG)
formatter = logging.Formatter(fmt='%(asctime)-15s %(processName)s %(levelname)s %(message)s',
                              datefmt='%d %b %y %H:%M:%S')


ch = logging.StreamHandler(sys.stderr)
ch.setFormatter(formatter)
ch.setLevel(logging.DEBUG)
log.addHandler(ch)

hand_re = re.compile("POST \/(.*) HTTP/1.1.*"
                     "user=([0-9]*).*"
                     "data=(.*)&"
                     "key=([a-zA-Z_0-9]*).*", re.DOTALL)

cfg = elliptics.Config()
cfg.config.check_timeout = 1000
cfg.config.wait_timeout = 1000
n = elliptics.Node(elliptics.Logger("/dev/stderr", 0), cfg)
n.add_remote("s16h.xxx.yandex.net", 1025)
s = elliptics.Session(n)
s.groups = [1]

activities = dict()


def process_packet(offsets, packet):
    global activities, s
    ret = True
    #print '[', packet, ']'
    hand, user, data, key = hand_re.search(packet).groups()
    #print hand, user, data, key

    user_key = user + '.' + key

    #print "user_key = ", user_key

    offset = 0
    size = len(data)
    if user_key in offsets:
        offset = offsets[user_key]
    #print ' user_key = [', user_key, '] offset = [', offset, '] size = [', size, ']'

    log.debug("Reading file '{0}' offset: {1} size: {2}".format(user_key, offset, size))
    r_data = s.read_data(user_key, offset, size)

    #print "r_data = ", r_data
    #print "data = ", data

    offsets[user_key] = offset + size

    if data != r_data:
        log.error("Error: data mismatch: [{0}] != [{1}]".format(data, r_data))
        ret = False

    if key not in activities:
        log.debug("Getting activity: {0}".format(key))
        activities[key] = [r.indexes[0].data
                           for r in s.find_any_indexes([key]).get()]
    activity = activities[key]
    #print 'activity size = ', len(activity)
    log.debug("Looking up for user: {0} in activity: {1}".format(user, key))
    if user not in activity:
        log.error("User: {0} isn't in activity: {1}".format(user, key))
        log.error("Activity: {0}".format(activity))
        ret = False

    return ret


def check_shoot(belt_path):
    offsets = dict()
    log.debug("Checking shoot: {0}".format(belt_path))
    with open(belt_path, 'r') as f:
        while True:
            try:
                line = f.readline()
                packet_size, _ = line.split(' ')
                packet_size = int(packet_size) + 1
		#print packet_size
                packet = f.read(packet_size)
                if not process_packet(offsets, packet):
                    return False
            except Exception as e:
                log.debug("Error: {0}".format(e))
                break


if __name__ == "__main__":
    if not check_shoot(sys.argv[1]):
        exit(1)
