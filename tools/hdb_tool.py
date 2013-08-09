#!/usr/bin/env python

import sys
import logging
import logging.handlers
from itertools import groupby

import elliptics
from elliptics_recovery.route import Address

log = logging.getLogger()
log.setLevel(logging.DEBUG)
formatter = logging.Formatter(fmt='%(asctime)-15s %(processName)s %(levelname)s %(message)s',
                              datefmt='%d %b %y %H:%M:%S')

ch = logging.StreamHandler(sys.stderr)
ch.setFormatter(formatter)
ch.setLevel(logging.INFO)
log.addHandler(ch)


def combine_logs(users, keys, new_key, batch_size, node, groups):
    log.debug("Creating session for reading and appending logs")
    log_session = elliptics.Session(node)
    log_session.ioflags = elliptics.io_flags.append
    log_session.cflags = elliptics.command_flags.nolock
    log_session.groups = groups

    log.debug("Creating session for reading and updating activity")
    activity_session = elliptics.Session(node)
    activity_session.cflags = elliptics.command_flags.nolock
    activity_session.groups = groups

    for key in keys:
        process_key(users=users,
                    key=key,
                    new_key=new_key,
                    batch_size=batch_size,
                    log_session=log_session,
                    activity_session=activity_session)


def process_key(users, key, new_key, batch_size, log_session, activity_session):
    log.debug("Processing key: {0}".format(key))
    try:
        results = activity_session.find_any_indexes([key]).get()
        log.debug("Found {0} users active at {1}".format(len(results), key))
        for batch_id, batch in groupby(enumerate((r.indexes[0].data for r in results)), key=lambda x: x[0] / batch_size):
            if users and len(users):
                batch = users.intersection([u for _, u in batch])
            else:
                batch = [u for _, u in batch]
            process_users(batch, key, new_key, log_session, activity_session)
    except Exception as e:
        log.error("Coudn't process key: {0} error: {1}".format(key, e))
    except:
        log.error("Coudn't process key: {0}".format(key))


def process_users(users, key, new_key, log_session, activity_session):
    users_len = len(users)
    log.debug("Processing users: {0} for key: {1}".format(users_len, key))

    if not users or len(users) == 0:
        log.debug("No users specified, skipping")
        return

    log.debug("Async updating indexes for {0} users for index: {1}".format(users_len, new_key))
    async_indexes = []
    for u in users:
        async_indexes.append(activity_session.set_indexes(elliptics.Id(u), [new_key], [u]))

    log.debug("Async reading logs for {0} users".format(users_len))
    async_read = log_session.bulk_read_async([elliptics.Id(u + '.' + key) for u in users])

    log.debug("Async writing read logs")
    async_writes = []
    it = iter(async_read)
    failed = 0
    while True:
        try:
            r = next(it)
            async_writes.append(log_session.write_data_async((r.id, elliptics.Time(-1, -1), 0), r.data), len(r.data))
        except StopIteration:
            break
        except Exception as e:
            failed += 1
            log.debug("Write failed: {0}".format(e))

    writed_bytes = 0
    for r, size in async_writes:
        r.wait()
        if r.successful():
            writed_bytes += size
        else:
            failed += 1

    log.info("Aggregated bytes: {0}".format(writed_bytes))
    log.info("Successed: {0}".format(users_len))
    log.info("Failures: {0}".format(failed))

    successes, failures = (0, 0)
    for a in async_indexes:
        a.wait()
        if a.successful():
            successes += 1
        else:
            failures += 1
    log.info("Activtiy updates successes: {0}".format(users_len))
    log.info("Activtiy updates failures: {0}".format(failed))


if __name__ == '__main__':
    from optparse import OptionParser

    parser = OptionParser()
    parser.usage = "%prog KEYS NEW_KEY [options]"
    parser.description = __doc__
    #parser.add_option("-n", "--nprocess", action="store", dest="nprocess", default="1",
    #                  help="Number of subprocesses [default: %default]")
    parser.add_option("-b", "--batch-size", action="store", dest="batch_size", default="1024",
                      help="Number of keys in read_bulk/write_bulk batch [default: %default]")
    parser.add_option("-d", "--debug", action="store_true", dest="debug", default=False,
                      help="Enable debug output [default: %default]")
    parser.add_option("-r", "--remote", action="store", dest="elliptics_remote", default=None,
                      help="Elliptics node address [default: %default]")
    parser.add_option("-g", "--groups", action="store", dest="elliptics_groups", default=None,
                      help="Comma separated list of groups [default: all]")
    parser.add_option("-l", "--log", dest="elliptics_log", default='hdb_tool.log', metavar="FILE",
                      help="Output log messages from library to file [default: %default]")
    parser.add_option("-L", "--log-level", action="store", dest="elliptics_log_level", default="1",
                      help="Elliptics client verbosity [default: %default]")
    parser.add_option("-u", "--user", action="append", dest="users", default=[],
                      help="User whose logs should be aggregated")

    (options, args) = parser.parse_args()

    ch.setLevel(logging.WARNING)
    if options.debug:
        ch.setLevel(logging.DEBUG)

    log.info('Initializing')

    try:
        log_file = options.elliptics_log
        log_level = int(options.elliptics_log_level)
    except Exception as e:
        raise ValueError("Can't parse log_level: '{0}': {1}".format(options.elliptics_log_level, e))
    log.info("Using elliptics client log level: {0}".format(log_level))

    try:
        batch_size = int(options.batch_size)
        if batch_size <= 0:
            raise ValueError("Batch size should be positive: {0}".format(batch_size))
    except Exception as e:
        raise ValueError("Can't parse batchsize: '{0}': {1}".format(options.batch_size, e))
    log.info("Using batch_size: {0}".format(batch_size))

    if not options.elliptics_remote:
        raise ValueError("Elliptics address should be given (-r option).")
    try:
        address = Address.from_host_port_family(options.elliptics_remote)
    except Exception as e:
        raise ValueError("Can't parse host:port:family: '{0}': {1}".format(options.elliptics_remote, e))
    log.info("Using host:port:family: {0}".format(address))

    if not options.elliptics_groups:
        raise ValueError("Elliptics groups should be given (-g option).")
    try:
        groups = map(int, options.elliptics_groups.split(','))
    except Exception as e:
        raise ValueError("Can't parse grouplist: '{0}': {1}".format(options.elliptics_groups, e))
    log.info("Using group list: {0}".format(groups))

    try:
        users = set(options.users)
    except Exception as e:
        raise ValueError("Can't parse keys: '{0}': {1}".format(options.users, e))
    log.info("Aggregating keys for users: 0".format(users))

    try:
        keys = args[0].split(':')
    except Exception as e:
        raise ValueError("Can't parse keys: '{0}': {1}".format(args[0], e))
    log.info("Aggregating keys: 0".format(keys))

    try:
        new_key = args[1]
    except Exception as e:
        raise ValueError("Can't parse new_key: '{0}': {1}".format(args[1], e))
    log.info("Aggregating keys in key: 0".format(new_key))

    log.info("Setting up elliptics client")

    log.debug("Creating logger")
    elog = elliptics.Logger(log_file, int(log_level))

    log.info("Creating node using: {0}".format(address))
    cfg = elliptics.Config()
    cfg.config.wait_timeout = 60
    cfg.config.check_timeout = 60
    cfg.config.io_thread_num = 16
    cfg.config.nonblocking_io_thread_num = 16
    cfg.config.net_thread_num = 16
    node = elliptics.Node(elog, cfg)
    node.add_remote(addr=address.host, port=address.port, family=address.family)
    log.info("Created node: {0}".format(node))

    log.info("Keys which will be aggregated: {0}".format(keys))
    log.info("New aggregated key: {0}".format(new_key))

    combine_logs(users, keys, new_key, batch_size, node, groups)
