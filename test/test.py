#!/usr/bin/env python

import random
from datetime import datetime
import json
from collections import defaultdict
from historydb import historydb
import logging
import logging.handlers
import sys
from time import sleep
import itertools

random.seed()

log = logging.getLogger(__name__)
log.setLevel(logging.DEBUG)
formatter = logging.Formatter(fmt='%(asctime)-15s %(levelname)s %(message)s',
                              datefmt='%d %b %y %H:%M:%S')

ch = logging.StreamHandler(sys.stderr)
ch.setFormatter(formatter)
ch.setLevel(logging.INFO)
log.addHandler(ch)

logs = defaultdict(str)
activity = defaultdict(list)

MAX_USER_NO = 30000000


def test_ping(host, iterations, debug):
    log.info("Run test_ping")
    hdb = historydb(host, debug)
    res = hdb.ping()
    if res == 200:
        log.info("Ping test successed")
    else:
        log.error("Ping failed: {0}".format(res))
        log.info("Ping test failed")
    return res == 200


def check_logs(hdb, user, keys=None, begin_time=None, end_time=None):
    cmp_logs = ''
    if keys:
        cmp_logs = ''.join([logs[user + x] for x in keys])
    else:
        cmp_logs = ''.join([logs[user + str(x)] for x in range(int(begin_time / (24 * 60 * 60)), int(end_time / (24 * 60 * 60)) + 1)])

    log.debug("Getting user '{0}' logs by keys: {1} and time period: [{2} - {3}]".format(user, keys, begin_time, end_time))
    resp = hdb.get_user_logs(user=user, keys=keys, begin_time=begin_time, end_time=end_time)
    log.debug("get_user_logs: {0}".format(resp))
    r_logs = ''
    if resp[0] != 200:
        log.error("Error while getting user logs")
        return False
    elif resp[1] != '':
        try:
            log.debug("JSON: '{0}'".format(resp[1]))
            r_logs = json.loads(resp[1])['logs']
        except Exception as e:
            log.error("Got exception: {0}".format(e))
            return False
    if r_logs != cmp_logs:
        log.error("Invalid logs from getter by timestamps: {0} != {1}".format(len(r_logs), len(cmp_logs)))
        return False
    else:
        log.debug("Logs from getter by timestamps is checked and it's OK")
        return True


def check_activity(hdb, keys=None, begin_time=None, end_time=None):
    log.debug("Getting users activity by keys: {0} and time period: [{1} - {2}]".format(keys, begin_time, end_time))
    resp = hdb.get_active_users(keys=keys, begin_time=begin_time, end_time=end_time)
    users = set()
    if keys:
        users = set(itertools.chain(*[activity[x] for x in keys]))
    else:
        users = set(itertools.chain(*[activity[x] for x in range(int(begin_time / (24 * 60 * 60)), int(end_time / (24 * 60 * 60)) + 1)]))

    r_users = set()
    if resp[0] != 200:
        log.error("Error while getting users activity by keys: {0} and time period: [{1} - {2}]".format(keys, begin_time, end_time))
        return False
    elif resp[1] != '':
        try:
            log.debug("JSON: '{0}'".format(resp[1]))
            r_users = set(json.loads(resp[1])['active_users'])
        except Exception as e:
            log.error("Got exception: {0}".format(e))
            return False
    if len(r_users.symmetric_difference(users)) != 0:
        log.error("Invalid activity: {0} != {1}".format(len(r_users), len(users)))
        return False
    else:
        log.debug("Activity is checked and it's OK")
        return True


def test_add_log(host, iterations, debug):
    global logs
    log.info("Run add_log test {0} times".format(iterations))
    result = True
    hdb = historydb(host, debug)

    user = "test_user_" + hex(random.randint(0, MAX_USER_NO))[2:]
    keys = set()

    begin_time = int(datetime.now().strftime('%s'))
    for _ in range(iterations):
        data = ''.join([hex(x)[2:] for x in random.sample(range(100), 100)])
        dt = datetime.now()
        if random.randint(0, 1) == 0:
            key = dt.strftime('%b_%d_%y')
            keys.add(key)
            log.debug("Writing '{0}' log to user '{1}' by key '{2}'".format(len(data), user, key))
            if hdb.add_log(user=user, data=data, key=key) != 200:
                log.error('Failed add log by key')
                result = False
            else:
                logs[user + key] += data
        else:
            time = int(dt.strftime("%s"))
            log.debug("Writing '{0}' log to user '{1}' by time '{2}'".format(len(data), user, time))
            if hdb.add_log(user=user, data=data, time=time) != 200:
                log.error('Failed add log by timestamp')
                result = False
            else:
                day = int(time / (24 * 60 * 60))
                logs[user + str(day)] += data
    end_time = int(datetime.now().strftime("%s"))

    log.info("Checking results")

    if not check_logs(hdb, user, keys=keys):
        result = False

    if not check_logs(hdb, user, begin_time=begin_time, end_time=end_time):
        result = False

    if result:
        log.info("Add_log test successed")
    else:
        log.info("Add_log failed")
    return result


def test_add_activity(host, iterations, debug):
    log.info("Run add_activity test for {0} times".format(iterations))
    result = True
    hdb = historydb(host, debug)
    keys = set()

    begin_time = int(datetime.now().strftime('%s'))
    for _ in range(iterations):
        user = "test_user_" + hex(random.randint(0, MAX_USER_NO))[2:]
        dt = datetime.now()
        if random.randint(0, 1) == 0:
            key = dt.strftime('%b_%d_%y')
            keys.add(key)
            if hdb.add_activity(user=user, key=key) != 200:
                log.error("Error while adding activity by keys")
            else:
                activity[key] += [user]
        else:
            time = int(dt.strftime("%s"))
            if hdb.add_activity(user=user, time=time) != 200:
                log.error("Error while adding activity by timestamp")
            else:
                day = int(time / (24 * 60 * 60))
                activity[day] += [user]
    end_time = int(datetime.now().strftime("%s"))

    log.info("Checking results")

    if not check_activity(hdb, keys=list(keys)):
        result = False

    if not check_activity(hdb, begin_time=begin_time, end_time=end_time):
        result = False

    if result:
        log.info("Add_activity test successed")
    else:
        log.info("Add_activity failed")
    return result


def test_add_log_with_activity(host, iterations, debug):
    log.info("Run add_log_with_activity test for {0} times".format(iterations))
    result = True
    hdb = historydb(host, debug)

    user = "test_user_" + hex(random.randint(0, MAX_USER_NO))[2:]
    keys = set()

    begin_time = int(datetime.now().strftime('%s'))
    for _ in range(iterations):
        data = ''.join([hex(x)[2:] for x in random.sample(range(100), 100)])
        dt = datetime.now()
        if random.randint(0, 1) == 0:
            key = dt.strftime('%b_%d_%y')
            keys.add(key)
            log.debug("Writing '{0}' log to user '{1}' by key '{2}'".format(len(data), user, key))
            if hdb.add_log_with_activity(user=user, data=data, key=key) != 200:
                log.error('Failed add log by key')
                result = False
            else:
                logs[user + key] += data
                activity[key] += [user]
        else:
            time = int(dt.strftime("%s"))
            log.debug("Writing '{0}' log to user '{1}' by time '{2}'".format(len(data), user, time))
            if hdb.add_log_with_activity(user=user, data=data, time=time) != 200:
                log.error('Failed add log by timestamp')
                result = False
            else:
                day = int(time / (24 * 60 * 60))
                logs[user + str(day)] += data
                activity[day] += [user]
    end_time = int(datetime.now().strftime("%s"))

    log.info("Checking results")

    if not check_logs(hdb, user, keys=keys):
        result = False

    if not check_logs(hdb, user, begin_time=begin_time, end_time=end_time):
        result = False

    if not check_activity(hdb, keys=keys):
        result = False

    if not check_activity(hdb, begin_time=begin_time, end_time=end_time):
        result = False

    if result:
        log.info("Add_log_with_activity test successed")
    else:
        log.info("Add_log_with_activity failed")
    return result


if __name__ == '__main__':
    from optparse import OptionParser
    from misc import start, stop
    import socket

    parser = OptionParser()
    parser.usage = "%prog [options]"
    parser.description = __doc__
    parser.add_option("-r", "--repeat", action="store", dest="repeat", default="1",
                      help="Number of test repeat [default: %default]")
    parser.add_option("-l", "--leave", action="store_true", dest="leave", default=False,
                      help="Leave tmp files [default: %default]")
    parser.add_option("-e", "--no-exit", action="store_true", dest="noexit", default=False,
                      help="Exit at the end [default: %default]")
    parser.add_option("-d", "--debug", action="store_true", dest="debug", default=False,
                      help="Turns on debug mode [default: %default]")
    parser.add_option("-i", "--iterations", action="store", dest="iterations", default=100,
                      help="Number of iterations of operation in test")
    parser.add_option("-D", "--dir", dest="tmp_dir", default='/var/tmp', metavar="DIR",
                      help="Temporary directory for data and logs [default: %default]")
    (options, args) = parser.parse_args()

    if options.debug:
        ch.setLevel(logging.DEBUG)

    start(host=socket.gethostname(), tmp_dir=options.tmp_dir)

    host = socket.gethostname() + ':8082'

    log.info("Starting tests")

    succ, fail = (0, 0)

    tests = []

    iterations = int(options.iterations)

    for _ in range(int(options.repeat)):
        tests.append(test_ping)
        tests.append(test_add_log)
        tests.append(test_add_activity)
        tests.append(test_add_log_with_activity)

    test_time = datetime.now()
    for t in tests:
        if t(host=host, iterations=iterations, debug=options.debug):
            succ += 1
        else:
            fail += 1
            log.error("TEST IS FAILED, stopping")
            options.leave = True
            break

    test_time = datetime.now() - test_time

    log.info("Ending tests")

    while options.noexit:
        try:
            sleep(10)
        except KeyboardInterrupt:
            pass
        except Exception as e:
            pass
        except:
            pass

    stop(leave=options.leave)

    print("{0:=^100}".format(" Results "))
    print('{0:<50}{1:>50}'.format('Successed test:', succ))
    print('{0:<50}{1:>50}'.format('Failed test:', fail))
    print('{0:<50}{1:>50}'.format('Elapsed time:', test_time))