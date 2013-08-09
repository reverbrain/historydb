#!/usr/bin/env python

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


def start(host):
    global root_dir, ioserv, thevoid

    root_dir = '/tmp/historydb-test-' + hex(random.randint(0, sys.maxint))[2:]
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
