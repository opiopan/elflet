import sys
import json
from optparse import OptionParser, OptionGroup
import elflet

functionModeMap = {
    'full': 'FullSpec',
    'sensor-only': 'SensorOnly'
}

irModeMap = {
    'on-demand': 'OnDemand',
    'continuous': 'Continuous'
}

def parser():
    usage = "usage: %prog -h|--help \n"\
            "       %prog [--raw|--stat] elflet-address\n"\
            "       %prog [-p PASSWORD] -j FILE elflet-address\n"\
            "       %prog [-p PASSWORD] [options] elflet-address"
    p = OptionParser(usage)

    # for get mode
    g = OptionGroup(p, 'Options for retrieving information')
    g.add_option('--raw',
                 action='store_true', dest='rawOutput',
                 help='show configuration as raw json data')
    g.add_option('--stat',
                 action='store_true', dest='stat',
                 help='show statistics data')
    p.add_option_group(g)

    # for updagte mode
    g = OptionGroup(p, 'Options for changing configuration')
    g.add_option('-p', '--password',
                 action='store', dest='password',
                 help="specify a admin password of elflet, "
                      "password will be asked interactively "
                      "if you ommit this option")
    g.add_option('-j', '--json',
                 action='store', dest='jsonFile', metavar='FILE',
                 help="specify a JSON file to reflect to elflet configuration")
    g.add_option('-n', '--name',
                 action='store', dest='name',
                 help="change elflet node name")
    g.add_option('-t', '--timezone',
                 action='store', dest='tz',
                 help="change timezone")
    g.add_option('-N', '--ntp',
                 action='store', dest='ntp',
                 help="change NTP server address")
    g.add_option('-m', '--mode',
                 action='store', dest='mode', choices=['full', 'sensor-only'],
                 help="change elflet function mode, MODE must be specified "
                      "eather 'full' or 'sensor-only'")
    g.add_option('-s', '--sensor-interval',
                 action='store', dest='sensorInterval', metavar='ITNERVAL',
                 type = 'int',
                 help="change sensor capturing interval time in second")
    g.add_option('-i', '--irmode',
                 action='store', dest='irmode', metavar='MODE',
                 choices=['on-demand', 'continuous'],
                 help="change IR reciever mode, MODE must be specified "
                      "eather 'on-demand' or 'continuous'")
    g.add_option('-b', '--enable-ble-keyboard',
                 action='store_true', dest='blehid',
                 help="enable BLE keyboard emurator function")
    g.add_option('-B', '--disable-ble-keyboard',
                 action='store_false', dest='blehid',
                 help="disable BLE keyboard emurator function")
    p.add_option_group(g)

    g = OptionGroup(p, 'MQTT PubSub related Options')
    g.add_option('--pubsub-server',
                 action='store', dest='pubsubServer', metavar='SERVER',
                 help="change PubSub server address "
                      "and enable PubSub function, "
                      "PubSub function will be disabled "
                      "if zero length string is specified")
    g.add_option('--pubsub-session',
                 action='store', dest='pubsubSession', metavar='TYPE',
                 choices=['TCP', 'TSL', 'WebSocket', 'WebSocketSecure'],
                 help="change type of session with PubSub server, "
                      "TYPE must be specified 'TCP', 'TSL', 'WebSocket' or "
                      "'WebSocketSecure'")
    g.add_option('--pubsub-user',
                 action='store', dest='pubsubUser', metavar='USER:PASS',
                 help="change user name and password to use authentication "
                      "in PubSub server")
    g.add_option('--pubsub-server-cert',
                 action='store', dest='pubsubCert', metavar='FILE',
                 help="change certification of PubSub server, "
                      "this is necessory when session type is "
                      "'TSL' or 'WebSocketSecure'")
    g.add_option('--sensor-topic',
                 action='store', dest='sensorTopic', metavar='TOPIC',
                 help="change topic name to publish sensor data")
    g.add_option('--shadow-topic',
                 action='store', dest='shadowTopic', metavar='TOPIC',
                 help="change topic name to publish shadow state")
    g.add_option('--irrc-recieved-data-topic',
                 action='store', dest='irrcRecievedDataTopic', metavar='TOPIC',
                 help="change topic name to publish recieved IR data")
    g.add_option('--irrc-recieve-topic',
                 action='store', dest='irrcRecieveTopic', metavar='TOPIC',
                 help="change topic name to subscribe request to "
                      "transit IR recieving mode")
    g.add_option('--irrc-send-topic',
                 action='store', dest='irrcSendTopic', metavar='TOPIC',
                 help="change topic name to subscribe transmitting IR data")
    g.add_option('--download-firmware-topic',
                 action='store', dest='downloadFirmwareTopic', metavar='TOPIC',
                 help="change topic name to subscribe request "
                      "to start firmware download")
    p.add_option_group(g)

    return p

def genBody(parser, options):
    body = {}
    if options.jsonFile != None:
        try:
            with open(options.jsonFile, 'r') as f:
                fdata = f.read()
                body = json.loads(fdata)
        except IOError as e:
            parser.error('{0}'.format(e))
        except:
            parser.error('invalid JSON format: {0}'.format(options.jsonFile))
    
    if options.name != None:
        body['NodeName'] = options.name
    if options.tz != None:
        body['Timezone'] = options.tz
    if options.ntp != None:
        body['NTPServer'] = options.ntp
    if options.mode != None:
        body['FunctionMode'] = functionModeMap[options.mode]
    if options.sensorInterval != None:
        body['SensorFrequency'] = options.sensorInterval
    if options.irmode != None:
        body['IrrcRecieverMode'] = irModeMap[options.irmode]
    if options.blehid != None:
        body['EnableBLEHID'] = options.blehid
    if options.pubsubServer != None:
        body['PubSubServerAddr'] = options.pubsubServer
    if options.pubsubSession != None:
        body['PubSubSessionType'] = options.pubsubSession
    if options.pubsubUser != None:
        arg = options.pubsubUser
        sep = arg.find(':')
        body['PubSubUser'] = arg[:sep] if sep >= 0 else arg
        body['PubSubPassword'] = arg[sep + 1:] if sep >= 0 else ''
    if options.pubsubCert != None:
        fname = options.pubsubCert
        try:
            with open(fname, 'r') as f:
                body['PubSubServerCert'] = f.read()
        except IOError as e:
            parser.error('{0}'.format(e))
    if options.sensorTopic != None:
        body['SensorTopic'] = options.sensorTopic
    if options.shadowTopic != None:
        body['ShadowTopic'] = options.shadowTopic
    if options.irrcRecievedDataTopic != None:
        body['IrrcRecievedDataTopic'] = options.irrcRecievedDataTopic
    if options.irrcRecieveTopic != None:
        body['IrrcRecieveTopic'] = options.irrcRecieveTopic
    if options.irrcSendTopic != None:
        body['IrrcSendTopic'] = options.irrcSendTopic
    if options.downloadFirmwareTopic != None:
        body['DownloadFirmwareTopic'] = options.downloadFirmwareTopic

    if len(body) > 0 and (options.rawOutput != None or
                          options.stat != None):
        parser.error('invalid options combination')
    return body

def printConfig(rdata):
    if sys.stdout.isatty():
        HEADER = '\033[96m'
        SUBHEADER = '\033[92m'
        ENDC = '\033[0m'
    else:
        HEADER = ''
        SUBHEADER = ''
        ENDC = ''
        
    pdata = rdata['PubSub']
    print HEADER + 'Board Information:' + ENDC
    print '    Hardware:               {0} ver. {1}'.\
        format(rdata['BoardType'], rdata['BoardVersion'])
    print '    Firmware Version:       {0}'.format(rdata['FirmwareVersion'])
    print '\n' + HEADER + 'Basic Configuration:' + ENDC
    print '    Node Name:              {0}'.format(rdata['NodeName'])
    print '    Connecting WiFi:        {0}'.format(rdata['SSID'])
    print '    NTP:                    {0} ({1})'.format(rdata['NTPServer'],
                                                         rdata['Timezone'])
    print '    Function Mode:          {0}'.format(rdata['FunctionMode'])
    print '    IR Reciever Mode:       {0}'.format(rdata['IrrcRecieverMode'])
    print '    MQTT Function:          {0}'.format('Enabled'
                                                   if pdata['PubSubServerAddr']
                                                   else 'Disabled')
    print '    BLE Keyboard Emulation: {0}'.format('Enabled'
                                                   if 'EnableBLEHID'
                                                   in rdata and
                                                   rdata['EnableBLEHID']
                                                   else 'Disabled')

    if not pdata['PubSubServerAddr']:
        return

    print '\n' + HEADER + 'MQTT Configuration:' + ENDC
    print '    MQTT server:            {0}'.format(pdata['PubSubServerAddr'])
    print '    Session Type:           {0}'.format(pdata['PubSubSessionType'])
    print '    User/Password:          {0}{1}'.format(pdata['PubSubUser'],
                                                      '/*****'
                                                      if pdata['PubSubUser']
                                                      else '')
    print '    Server Certification:   {0}\n'.format('Registered'
                                                     if
                                                     pdata['PubSubServerCert']
                                                     else 'Unregistered')
    print SUBHEADER + '    Publishing Topic:' + ENDC
    print '        Sensor Value:       {0}'.format(pdata['SensorTopic'])
    print '        Recieved IR Data:   {0}'.\
        format(pdata['IrrcRecievedDataTopic'])
    print '        Shadow Status:      {0}\n'.format(pdata['ShadowTopic'])
    print SUBHEADER + '    Subscribed Topic:' + ENDC
    print '        Start IR recieving: {0}'.format(pdata['IrrcRecieveTopic'])
    print '        Send IR data:       {0}'.format(pdata['IrrcSendTopic'])
    print '        Download Firmware:  {0}'.\
        format(pdata['DownloadFirmwareTopic'])
    
def getInformation(options, host):
    raw = (options.stat or options.rawOutput)
    path = '/manage/status' if options.stat else '/manage/config'
    result, rdata = elflet.request(host, path, method = elflet.GET)
    if result:
        if raw:
            print json.dumps(rdata, indent = 4, sort_keys = True)
        else:
            printConfig(rdata)
    else:
        print >> sys.stderr, \
            "an error occurred while communicating with elflet: {0}" \
            .format(rdata)
        sys.exit(1)

def changeConfig(data, host, password):
    data['commit'] = True
    result, rdata = elflet.request(host, '/manage/config',
                                   method = elflet.POST, data = data,
                                   password = password)
    if not result:
        print >> sys.stderr, \
            "an error occurred while communicating with elflet: {0}" \
            .format(rdata)
        sys.exit(1)

def main():
    p = parser()
    options, args = p.parse_args()
    if len(args) != 1:
        p.error('one argument must be specified as elflet address')
    host = args[0]
    body = genBody(p, options)
    if len(body) > 0:
        changeConfig(body, host, options.password)
    else:
        getInformation(options, host)
