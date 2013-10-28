#!/usr/bin/env python

# =============================================================================
#  Copyright 2013+ Kirill Smorodinnikov <shaitkir@gmail.com>
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
# =============================================================================


import sys
import elliptics
import logging
import logging.handlers
import re
import mmap
from datetime import datetime

log = logging.getLogger()
log.setLevel(logging.DEBUG)
formatter = logging.Formatter(fmt='%(asctime)-15s %(processName)s '
                              '%(levelname)s %(message)s',
                              datefmt='%d %b %y %H:%M:%S')


ch = logging.StreamHandler(sys.stderr)
ch.setFormatter(formatter)
ch.setLevel(logging.INFO)
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

activities = dict()
offsets = dict()

counter = 0
start = None


def process_packet(packet, s, perc):
    global activities, offsets, counter, start
    ret = True
    log.debug("packet: '{0}'".format(packet))
    hand, user, data, key = hand_re.search(packet).groups()
    log.debug("hand: '{0}', user = '{1}', data = '{2}', key = '{3}'"
              .format(hand, user, data, key))

    user_key = user + '.' + key

    offset = 0
    size = len(data)
    if user_key in offsets:
        offset = offsets[user_key]
    log.debug("user_key = '{0}' offset = '{1}' size = '{2}'"
              .format(user_key, offset, size))

    log.debug("Reading file '{0}' offset: {1} size: {2}"
              .format(user_key, offset, size))
    r_data = s.read_data(user_key, offset, size).get()[0].data

    offsets[user_key] = offset + size

    if data != r_data:
        log.error("Error: user_key: '{2}' offset: '{3}' size: '{4}'"
                  " data mismatch: [{0}] != [{1}]"
                  .format(data, r_data, user_key, offset, size))
        ret = False

    if key not in activities:
        log.debug("Getting activity: {0}".format(key))
        activities[key] = set([r.indexes[0].data
                               for r in s.find_any_indexes([key]).get()])
    activity = activities[key]
    log.debug("Looking up for user: {0} in activity: {1}. Activity size = {2}"
              .format(user, key, len(activity)))
    if user not in activity:
        log.error("User: {0} isn't in activity: {1}".format(user, key))
        log.error("Activity: {0}".format(activity))
        ret = False

    counter += 1

    if counter > 999:
        log.info("Speed: {0} {1}%"
                 .format(counter / (datetime.now() - start).total_seconds(),
                         perc))
        start = datetime.now()
        counter = 0

    return ret


def check_shoot(belt_path):
    global start
    import os
    log.debug("Checking shoot: {0}".format(belt_path))
    ret = True
    s = elliptics.Session(n)
    s.groups = [1]
    start = datetime.now()
    with open(belt_path, 'r') as f:
        fsize = os.fstat(f.fileno()).st_size
        mm = mmap.mmap(f.fileno(), 0, prot=mmap.PROT_READ)
        while True:
            try:
                line = mm.readline()
                packet_size, _ = line.split(' ')
                packet_size = int(packet_size) + 1
                packet = mm.read(packet_size)
                if not process_packet(packet, s, mm.tell() * 100.0 / fsize):
                    ret = False
            except Exception as e:
                log.debug("Error: {0}".format(e))
                break
        mm.close()
    return ret


if __name__ == "__main__":
    ret = True
    for arg in sys.argv[1:]:
        if not check_shoot(arg):
            ret = False

    if not ret:
        exit(1)
