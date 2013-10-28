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


class historydb(object):
    def __init__(self, addr, debug=False):
        self.addr = addr
        if debug:
            self.debug_level = 10
        else:
            self.debug_level = 0

    def ping(self):
        res = self.__send__({}, "/")
        if res is None:
            return 500
        return res.status

    def add_log(self, user, data, time=None, key=None):
        p = {"user" : user, "data" : data}
        if time:
                p['time'] = time
        elif key:
                p['key'] = key
        else:
                return 500
        res = self.__send__(p, "/add_log")
        if res is None:
            return 500
        return res.status

    def add_activity(self, user, time=None, key=None):
        p = {"user" : user}
        if not time is None:
                p['time'] = time
        elif key:
                p['key'] = key
        else:
                return
        res = self.__send__(p, "/add_activity")
        return res.status

    def add_log_with_activity(self, user, data, time=None, key=None):
        p = {"user" : user, "data" : data}
        if time:
                p['time'] = time
        elif key:
                p['key'] = key
        else:
                return
        res = self.__send__(p, "/add_log_with_activity")
        if res is None:
            return (500, "")
        return res.status

    def get_user_logs(self, user, keys=None, begin_time=None, end_time=None):
        p = {'user' : user}
        if keys:
                p["keys"] = ':'.join(keys)
        elif begin_time and end_time:
                p['begin_time'] = begin_time
                p['end_time'] = end_time
        else:
                return
        res = self.__send__(p, "/get_user_logs", "GET")
        if res is None:
            return (500, "")
        return (res.status, res.read(), res.reason)

    def get_active_users(self, begin_time=None, end_time=None, keys=None):
        p = {}
        if keys:
                p["keys"] = ':'.join(keys)
        elif begin_time is not None and end_time is not None:
                p["begin_time"] = begin_time
                p['end_time'] = end_time
        else:
                return
        res = self.__send__(p, "/get_active_users", "GET")
        if res is None:
            return (500, "")
        return (res.status, res.read(), res.reason)

    def __send__(self, params, url, method="POST"):
        try:
            from httplib import HTTPConnection
            from urllib import urlencode
            h = HTTPConnection(self.addr)
            h.set_debuglevel(self.debug_level)
            body = urlencode(params)
            if method == "GET":
                url += "?" + body
                body = None
            h.request(method, url, body)
            response = h.getresponse()
            return response
        except Exception as e:
            print "Got exception: {0}".format(e)
            return None
