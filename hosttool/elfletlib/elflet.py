import sys
import requests
from requests.auth import HTTPDigestAuth
from getpass import getpass

HTTPTIMEOUT = 10

GET = 1
POST = 2
DELETE = 3

def request(host, path, method = GET, data = None, password = None):
    url = 'http://' + host + path
    if method == GET:
        proc = requests.get
    elif method == POST:
        proc = requests.post
    elif method == DELETE:
        proc = requests.delete
    else:
        return False, 'invalid method is specified'
    
    def doreq():
        auth = HTTPDigestAuth('admin', password) if password else None
        body = data
        resp = proc(url, json = body, auth = auth, timeout = HTTPTIMEOUT)
        ctype = resp.headers['Content-Type'] \
                if 'Content-Type' in resp.headers else None
        return (resp.status_code,
                resp.json() if ctype == 'application/json' else resp.text)

    try:
        code, rdata = doreq()
        if code == 401 and sys.stdout.isatty() and sys.stdin.isatty() \
           and not password:
            password = getpass('password: ')
            code, rdata = doreq()
            
        if code == 401:
            rdata = 'authorization failed'
            
        return (code == 200, rdata)

    except requests.ConnectionError as e:
        return (False, 'cannot connect to "{0}"'.format(host))
