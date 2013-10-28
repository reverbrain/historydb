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


import elliptics
from sets import Set

TOOL_COMBINE = "combine"
TOOL_SET = (TOOL_COMBINE,)

SECONDS_IN_DAY = 24 * 60 * 60


def combine_logs(remotes, groups, min_write, keys, new_key):
    elog = elliptics.Logger("/dev/stderr", 0)
    cfg = elliptics.Config()
    cfg.config.wait_timeout = 60
    cfg.config.check_timeout = 60
    cfg.config.io_thread_num = 16
    cfg.config.nonblocking_io_thread_num = 16
    cfg.config.net_thread_num = 16

    node = elliptics.Node(elog, cfg)

    for r in remotes:
        try:
            node.add_remote(addr=r[0], port=r[1], family=r[2])
        except Exception as e:
            print "Coudn't connect to elliptics node: {0}: {1}".format(r, e)

    log_s = elliptics.Session(node)
    log_s.set_groups(groups)
    log_s.set_ioflags(elliptics.io_flags.append)

    index_s = elliptics.Session(node)
    index_s.set_groups(groups)
    index_s.set_ioflags(elliptics.io_flags.cache)

    users = Set()

    print "Keys: {0}".format(keys)

    for key in keys:
        try:
            users.update(process_key(key, log_s, index_s, new_key))
        except Exception as e:
            print "Process key failed: {0}".format(e)

    print "Users: {0}".format(users)

    for u in users:
        try:
            index_s.update_indexes(elliptics.Id(u), [new_key + ".0"], [u])
        except Exception as e:
            print "Update_indexes failed: {0}".format(e)


def process_key(key, log_s, index_s, new_key):
    res = index_s.find_any_indexes([key + str(x) for x in range(1000)])
    users = Set()
    for r in res:
        for ind, data in r.indexes:
            users.add(data)

    print "Users: {0}".format(users)

    async_reads = []

    for u in users:
        try:
            k = u + "." + key
            print "Read latest: {0}".format(k)
            async_reads.append((log_s.read_latest(k), u))
        except Exception as e:
            print "Read latest async failed: {0}".format(e)

    for r, u in async_reads:
        try:
            k = u + "." + new_key
            r.wait()
            result = r.get()[0]
            print "Write: {0}".format(k)
            io = elliptics.IoAttr()
            io.id = elliptics.Id(k)
            io.timestamp = elliptics.Time(0, 0)
            io.user_flags = 0
            write_result = log_s.write_data(io, result.data)
            write_result.wait()
            print "Write is {0}".format(write_result.successful())
        except Exception as e:
            print "Write data async failed: {0}".format(e)

    return users

if __name__ == '__main__':
    from optparse import OptionParser
    parser = OptionParser()
    parser.usage = "%prog TOOL [options]"
    parser.description = __doc__
    parser.add_option("-t", "--time-period", action="store", dest="time", default=None,
                      help="Time period of user logs [format: begin_time:end_time]")
    parser.add_option("-k", "--keys", action="store", dest="keys", default=None,
                      help="Custom subkeys of user logs and activity separated by ':'")
    parser.add_option("-r", "--remote", action="append", dest="elliptics_remote",
                      help="Elliptics node address [format: host:port:family]")
    parser.add_option("-n", "--new-key", action="store", dest="new_key", default=None,
                      help="New subkey for combined users logs and activity")
    parser.add_option("-g", "--group", action="append", dest="groups", default=[],
                      help="Elliptics groups separated by ':'")
    parser.add_option("-m", "--min_writes", action="store", dest="min_writes", default=1,
                      help="Minimum of successed writes [default %default]")

    (options, args) = parser.parse_args()

    remotes = []

    for r in options.elliptics_remote:
        s = r.split(":")
        remotes.append((s[0], int(s[1]), int(s[2])))

    keys = []

    if options.time:
        begin, end = options.time.split(":")
        begin = int(begin) / SECONDS_IN_DAY
        end = int(end) / SECONDS_IN_DAY
        for i in range(begin, end + 1):
            keys.append(str(i))

    if options.keys:
        keys = options.keys.split(":")

    if not len(keys):
        raise ValueError("Time and keys aren't specified")

    groups = []

    if options.groups:
        for g in options.groups:
            groups.extend(g.split(":"))

    new_key = options.new_key

    if new_key is None:
        raise ValueError("New key isn't specified")

    if not len(groups):
        raise ValueError("Groups aren't specified")

    min_writes = min(int(options.min_writes), len(groups))

    print remotes, groups, min_writes, keys, new_key

    combine_logs(remotes, groups, min_writes, keys, new_key)
