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


import random
from subprocess import Popen
import os
import sys
from shutil import rmtree
from time import sleep

random.seed()

ioserv, thevoid, root_dir = (None, None, None)


def output_configs(host):
    e_str_conf = '''log = {0}/historydb-elliptics.log
log_level = 5
group = 1
history = {0}
io_thread_num = 250
net_thread_num = 100
nonblocking_io_thread_num = 50
join = 1
remote =  {1}:2025:2
addr = {1}:2025:2
wait_timeout = 30
check_timeout = 50
auth_cookie = unique_storage_cookie
cache_size = 1024
server_net_prio = 0x20
client_net_prio = 6
flags = 8
backend = blob
blob_size = 10G
records_in_blob = 5000000
blob_flags = 2
blob_cache_size = 0
defrag_timeout = 3600
defrag_percentage = 25
sync = -1
data = {0}/data
iterate_thread_num = 2'''.format(root_dir, host)
    e_conf = open(root_dir + '/elliptics.conf', "w+")
    e_conf.write(e_str_conf)
    h_str_json = '''{{
    "endpoints": [
        "0.0.0.0:8082"
    ],
    "daemon": {{
        "monitor-port": 20000
    }},
    "backlog": 128,
    "threads": 2,
    "application": {{
        "loglevel": 5,
        "logfile": "{0}/historydb-test.log",
        "remotes": [
            "{1}:2025:2"
        ],
        "groups": [
            1
        ]
    }}
}}'''.format(root_dir, host)
    h_json = open(root_dir + '/historydb.json', "w+")
    h_json.write(h_str_json)


def start(host, tmp_dir):
    global root_dir, ioserv, thevoid

    if not os.path.exists(tmp_dir):
        try:
            os.makedirs(tmp_dir, 0755)
        except Exception as e:
            raise ValueError("Directory: {0} does not exist and could not be created: {1}".format(tmp_dir, e))

    os.chdir(tmp_dir)

    root_dir = os.path.join(tmp_dir, 'historydb-test-' + hex(random.randint(0, sys.maxint))[2:])
    os.mkdir(root_dir)
    output_configs(host=host)

    ioserv = Popen(['dnet_ioserv', '-c', root_dir + '/elliptics.conf'])
    sleep(0.5)
    thevoid = Popen(['historydb-thevoid', '-c', root_dir + '/historydb.json'])
    sleep(0.5)


def stop_app(app):
    import signal

    app.send_signal(signal.SIGINT)
    if app.poll():
        sleep(5)
        if app.poll():
            print 'Kill after 5 sec timeout'
            app.kill()


def stop(leave=False):
    global ioserv, thevoid

    stop_app(ioserv)
    ioserv = None

    stop_app(thevoid)
    thevoid = None

    if not leave:
        rmtree(root_dir, True)
