# Copyright (C) 2017 Bengt Martensson.

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or (at
# your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with
# this program. If not, see http://www.gnu.org/licenses/.

"""
This is a new and independent implementation of the Lirc irsend(1) program.
It offers a Python API and a command line interface. The command line
interface is almost, but not quite, compatible with irsend. Instead, it is
organized as a program with subcommands, send_once, etc.

There are some other subtile differences from irsend:

* subcommand must be lower case,
* send_once only takes one command (irsend takes several),
* send_stop without arguments uses the remote and the command from the last
  send_start command,
* no need to give dummy empty arguments for list,
* The --count argument to send_once is argument to the subcommand.
* the code in list remote is suppressed, unless -c is given,
* port number must be given with the --port (-p) argument; hostip:portnumber
  is not recognized,
* verbose option --verbose (-v)
* selectable timeout with --timeout (-t) option
* better error messages

It is using the new lirc Python API, including a C extension module.

For a GUI version, look at IrScrutinizer.
For a Java version, look at Javairtool
https://github.com/bengtmartensson/JavaLircClient
"""

import argparse
import socket
import sys

import client

_DEFAULT_PORT = 8765


def _parse_commandline():
    ''' Parse the command line, returns a filled-in  parser. '''
    # pylint: disable=bad-continuation
    parser = argparse.ArgumentParser(
        prog='irtool',
        description="Tool to send IR codes and manipulate lircd(8)")
    parser.add_argument(
        "-a", "--address",
        help='lircd host IP name or address, overrides --device.',
        metavar="host", dest='address', default=None)
    path = client.get_default_socket_path()
    parser.add_argument(
        '-d', '--device',
        help='lircd socket path [%s]' % path, metavar="path",
        dest='socket_pathname', default=None)
    parser.add_argument(
        '-p', '--port',
        help='lircd IP port, use with --address [%d] ' % _DEFAULT_PORT,
        dest='port', default=_DEFAULT_PORT, type=int)
    parser.add_argument(
        '-t', '--timeout',
        help='Timeout in milliseconds [No timeout]', metavar="ms",
        dest='timeout', type=int, default=None)
    parser.add_argument(
        '-V', '--version',
        help='Display version information for irtool',
        dest='versionRequested', action='store_true')
    parser.add_argument(
        '-v', '--verbose',
        help='Have some commands executed verbosely',
        dest='verbose', action='store_true')
    subparsers = parser.add_subparsers(
        dest='subcommand',
        metavar='sub-commands')

    # Command send_once
    parser_send_once = subparsers.add_parser(
        'send-once',
        help='Send one command')
    parser_send_once.add_argument(
        '-#', '-c', '--count',
        help='Number of times to send command in send_once',
        dest='count', type=int, default=1)
    parser_send_once.add_argument('remote', help='Name of remote')
    parser_send_once.add_argument('command', help='Name of command')

    # Command send_start
    parser_send_start = subparsers.add_parser(
        'send-start',
        help='Start sending one command until stopped')
    parser_send_start.add_argument(
        'remote',
        help='Name of remote')
    parser_send_start.add_argument(
        'command',
        help='Name of command')

    # Command send_stop
    parser_send_stop = subparsers.add_parser(
        'send-stop',
        help='Stop sending the command from send_start')
    parser_send_stop.add_argument(
        'remote',
        help='remote command')
    parser_send_stop.add_argument(
        'command',
        help='remote command')

    # Command list-remotes
    subparsers.add_parser('list-remotes', help='List available remotes')

    # Command list-keys
    parser_list_keys = subparsers.add_parser(
        'list-keys',
        help='list defined keys in given remote')
    parser_list_keys.add_argument(
        'remote',
        help='Name of remote')
    parser_list_keys.add_argument(
        "-c", "--codes",
        help='List the numerical codes in lircd.conf, not just names',
        dest='codes', action='store_true')

    # Command driver-option
    parser_drv_option = subparsers.add_parser(
        'driver-option',
        help='Set driver option to given value')
    parser_drv_option.add_argument('option', help='Option name')
    parser_drv_option.add_argument('value', help='Option value')

    # Command set_input_logging
    parser_set_input_log =  \
        subparsers.add_parser('set-inputlog', help='Set input logging')
    parser_set_input_log.add_argument(
        'log_file', nargs='?',
        help='Path to log file, empty to inhibit logging', default='')

    # Command set_driver_options
    parser_set_driver_options = subparsers.add_parser(

        'set-driver-options',
        help='Set driver options')
    parser_set_driver_options.add_argument('key', help='Option name')
    parser_set_driver_options.add_argument('value', help='Option value')

    # Command get version
    subparsers.add_parser('version', help='Get lircd version')

    # Command simulate
    parser_simulate = subparsers.add_parser(
        'simulate',
        help='Fake the reception of IR signals')
    parser_simulate.add_argument(
        'remote',
        help='remote part of simulated event')
    parser_simulate.add_argument(
        'key',
        help='Name of command to be faked')
    parser_simulate.add_argument(
        'data',
        help='Key press data to be sent to the Lircd')

    # Command set_transmitters
    parser_set_transmitters = subparsers.add_parser(
        'set-transmitters',
        help='Set transmitters')
    parser_set_transmitters.add_argument(
        'transmitters',
        metavar='N', nargs='+', help="transmitter...")

    args = parser.parse_args()

    if args.versionRequested:
        print("@version@")
        sys.exit(0)
    return args


def _send_once_command(connection, args):
    ''' Perform a SEND_ONCE ... socket command. '''
    if isinstance(args.keys, str):
        args.keys = [args.keys]
    command = client.SendCommand(connection, args.remote, args.keys)
    parser = command.run(args.timeout)
    if not parser.success:
        print(parser.data[0])
    return 0 if parser.success else 1


def _start_repeat_command(conn, args):
    ''' Perform a  SEND_START <remote> <key> socket command. '''
    command = client.StartRepeatCommand(conn, args.remote, args.key)
    parser = command.run(args.timeout)
    if not parser.success:
        print(parser.data[0])
    return 0 if parser.success else 1


def _stop_repeat_command(conn, args):
    ''' Perform a  SEND_STOP <remote> <key> socket command. '''
    command = client.StopRepeatCommand(conn, args.remote, args.key)
    parser = command.run(args.timeout)
    if not parser.success:
        print(parser.data[0])
    return 0 if parser.success else 1


def _drv_option_command(conn, args):
    ''' Perform a "DRV_OPTION <option> <value>" socket command. '''
    command = client.DrvOptionCommand(conn, args.option, args.value)
    parser = command.run(args.timeout)
    if not parser.success:
        print(parser.data[0])
    return 0 if parser.success else 1


def _list_keys_command(conn, args):
    ''' Perform a irsend LIST <remote> socket command. '''
    command = client.ListKeysCommand(conn, args.remote)
    parser = command.run(args.timeout)
    if not parser.success:
        print(parser.data[0])
    else:
        if not args.codes and args.remote:
            parser.data = [x.split()[-1] for x in parser.data]
        for key in parser.data:
            print(key)
    return 0 if parser.success else 1


def _list_remotes_command(conn):
    ''' Perform a irsend LIST command. '''
    command = client.ListRemotesCommand(conn)
    parser = command.run()
    if not parser.success:
        print(parser.data[0])
    else:
        for key in parser.data:
            print(key)
    return 0 if parser.success else 1


def _set_input_log_command(conn, args):
    ''' Start or stop lircd logging using SET_LOGFILE socket command. '''
    command = client.SetLogCommand(conn, args.logfile)
    parser = command.run(args.timeout)
    if not parser.success:
        print(parser.data[0])
    return 0 if parser.success else 1


def _simulate_command(conn, args, repeat=0):
    ''' Roughly a irsend SIMULATE equivalent. '''
    command = \
        client.SimulateCommand(
            conn, args.remote, args.key, repeat, args.data)
    parser = command.run(args.timeout)
    if not parser.success:
        print(parser.data[0])
    return 0 if parser.success else 1


def _transmitters_cmd(conn, args):
    ''' Perform an irsend SET_TRANSMITTERS command. '''
    command = client.SetTransmittersCommand(conn, args.transmitters)
    parser = command.run(args.timeout)
    if not parser.success:
        print(parser.data[0])
    return 0 if parser.success else 1


def _version_command(conn):
    ''' Retrieve lircd version using the VERSION socket command. '''
    command = client.VersionCommand(conn)
    parser = command.run()
    print(parser.data[0])
    return 0 if parser.success else 1


def _not_implemented():
    ''' Indeed, the not-implemented message '''
    print("Subcommand not implemented yet, are YOU volunteering?")
    return 2


def main():
    ''' Indeed: main function. '''

    args = _parse_commandline()
    if args.address:
        s = socket.socket((socket.AF_INET, socket.SOCK_STREAM))
        s.connect((args.address, args.port))
        conn = client.CommandConnection(s)
    else:
        conn = client.CommandConnection(args.socket_pathname)

    cmd_table = {
        'send-once':
            lambda: _send_once_command(conn, args),
        'send-start':
            lambda: _start_repeat_command(conn, args),
        'send-stop':
            lambda: _stop_repeat_command(conn, args),
        'list-keys':
            lambda: _list_keys_command(conn, args),
        'list-remotes':
            lambda: _list_remotes_command(conn),
        'set-inputlog':
            lambda: _set_input_log_command(conn, args),
        'simulate':
            lambda: _simulate_command(conn, args),
        'set-transmitters':
            lambda: _transmitters_cmd(conn, args),
        'driver-option':
            lambda: _drv_option_command(conn, args),
        'version':
            lambda: _version_command(conn),
    }
    try:
        if args.subcommand in cmd_table:
            exitstatus = cmd_table[args.subcommand]()
        else:
            print('Unknown subcommand, use --help for syntax.')
            exitstatus = 3

    except ConnectionRefusedError:
        print("Connection refused")
        exitstatus = 5
    except FileNotFoundError:
        print("Could not find {0}".format(args.socket_pathname))
        exitstatus = 5
    except PermissionError:
        print("No permission to open {0}".format(args.socket_pathname))
        exitstatus = 5

    sys.exit(exitstatus)


if __name__ == "__main__":
    main()
