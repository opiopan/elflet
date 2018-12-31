import sys
import json
from optparse import OptionParser, OptionGroup
import elflet

LIST = 0
SHOW = 1
CHANGE = 2
SHOWDEF = 3
ADDDEF = 4
DELDEF = 5

def parse():
    usage = "usage: %prog -h|--help \n"\
            "       %prog [--list] elflet-address\n"\
            "       %prog [--show] [--raw] elflet-address shadow-name\n"\
            "       %prog --change {-d JSON | -f FILE} "\
            "elflet-address shadow-name\n"\
            "       %prog --show-def elflet-address shadow-name\n"\
            "       %prog --add-def {-d JSON | -f FILE} "\
            "elflet-address shadow-name\n"\
            "       %prog --delete-def elflet-address shadow-name"
    p = OptionParser(usage)

    g = OptionGroup(p, 'Operation mode')
    g.add_option('-l', '--list',
                 action='store_const', dest='mode', const=LIST,
                 help='list registered shadow names, '
                      'this option can be ommited')
    g.add_option('-s', '--show',
                 action='store_const', dest='mode', const=SHOW,
                 help='show shadow status, '
                      'this option can be ommited')
    g.add_option('-c', '--change',
                 action='store_const', dest='mode', const=CHANGE,
                 help='change shadow status, '
                      'this option can be ommited')
    g.add_option('-S', '--show-def',
                 action='store_const', dest='mode', const=SHOWDEF,
                 help='show shadow definition')
    g.add_option('-A', '--add-def',
                 action='store_const', dest='mode', const=ADDDEF,
                 help='register shadow definition, '
                      'shadow definition will be overwriten '
                      'if specified shadow name is already registered')
    g.add_option('-D', '--delete-def',
                 action='store_const', dest='mode', const=DELDEF,
                 help='unregister shadow definition')
    p.add_option_group(g)
    
    g = OptionGroup(p, 'Other options')
    g.add_option('-r', '--raw',
                 action='store_true', dest='raw',
                 help='show shadow status as raw JSON format, '
                      'this option is effective when command will work as '
                      '"show" mode')
    g.add_option('-p', '--password',
                 action='store', dest='password', metavar='PASSWORD',
                 help='administorator password of elflet')
    g.add_option('-d', '--data',
                 action='store', dest='data', metavar='JSON',
                 help='JSON string data to upload to elflet')
    g.add_option('-f', '--file',
                 action='store', dest='file', metavar='FILE',
                 help='file path containing JSON data to upload '
                      'to elflet')
    p.add_option_group(g)

    options, args = p.parse_args()
    host = args[0] if len(args) > 0 else None
    shadow = args[1] if len(args) > 1 else None

    if not host:
        p.error('elflet address must be specified')
    if options.data != None and options.file != None:
        p.error('-d option and -f option cannot be specified concurrently')
    if options.mode == None:
        options.mode = LIST if not shadow else SHOW
    if (options.mode == CHANGE or options.mode == ADDDEF) and \
       (not options.data and not options.file):
        p.error(('-d or -f option is nessesary when {0} option is '
                'specified').format('-c or --change'
                                    if options.mode == CHANGE else
                                    '-A or --add-def'))
    if options.file != None:
        try:
            with open(options.file, 'r') as f:
                options.data = f.read()
        except IOError as e:
            print >> sys.stderr, 'elflet-shadow: {0}'.format(e)
            sys.exit(1)

    if options.data != None:
        try:
            jdata = json.loads(options.data)
            options.data = jdata
        except:
            print >> sys.stderr, \
                'elflet-shadow: specified data is invalid JSON format'
            sys.exit(1)
        
    return options, host, shadow

def listShadow(options, host, shadow):
    result, data = elflet.request(host, '/shadow')
    if result:
        print '{0} shadows are registered'.format(len(data))
        for name in data:
            print '    {0}'.format(name)

    return result, data

def showStatus(options, host, shadow):
    result, data = elflet.request(host, '/shadow/{0}'.format(shadow))
    if result:
        if options.raw:
            print json.dumps(data, indent=4, sort_keys=True)
        else:
            print 'Node Name:    {0}'.format(data['NodeName'])
            print 'Shadow Name:  {0}'.format(data['ShadowName'])
            print 'Power Status: {0}'.format('ON' if data['IsOn'] else 'OFF')

            if 'Attributes' in data:
                attrs = data['Attributes']
                if len(attrs) > 0:
                    print 'Atrributes:'
                    maxkeylen = 0
                    for key in attrs:
                        maxkeylen = max(maxkeylen, len(key))
                        fstr = '    {0:' + str(maxkeylen + 1) + 's}: {1}'
                    for key in attrs:
                        print fstr.format(key, attrs[key])
                
    return result, data
    
def changeStatus(options, host, shadow):
    return elflet.request(host, '/shadow/{0}'.format(shadow),
                          method = elflet.POST, data = options.data)
    
def showDef(options, host, shadow):
    result, data = elflet.request(host, '/shadowDefs/{0}'.format(shadow))
    if result:
        print json.dumps(data, indent=4, sort_keys=True)
                
    return result, data
    
def addDef(options, host, shadow):
    return elflet.request(host, '/shadowDefs/{0}'.format(shadow),
                          method = elflet.POST, data = options.data,
                          password = options.password)
    
def delDef(options, host, shadow):
    return elflet.request(host, '/shadowDefs/{0}'.format(shadow),
                          method = elflet.DELETE,
                          password = options.password)

operations =[listShadow, showStatus, changeStatus, showDef, addDef, delDef]

def main():
    options, host, shadow = parse()
    result, data = operations[options.mode](options, host, shadow)
    if not result:
        print >> sys.stderr, 'elflet-shadow: {0}'.format(data)
        sys.exit(1)
