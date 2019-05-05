''' Top-level python bindings for the lircd socket interface. '''
##
#   @file client.py
#   @author Alec Leamas
#   @brief Python bindings for a subset of the lirc_client.h interface.
#   @ingroup  python_bindings

##
#   @defgroup python_bindings   Python client bindings
#
#   Unstable python interfaces to read and send lirc data.
#
#   Sending is a pure python implementation described in @ref sending.
#
#   Reading data uses a C extension module, see @ref receiving. This also
#   includes AsyncConnection with a small asynchronous interface to read data.
#
#   The otherwise undocumented file config.py, which can be imported using
#   <i>import lirc.config</i>,  provides access to the paths defined when
#   running configure such as VARRUNDIR (often /var/run) and SYSCONFDIR
#   (typically /etc).

#   pylint: disable=W0613

##  @addtogroup python_bindings
#   @{

from abc import ABCMeta, abstractmethod
from enum import Enum
import configparser
import os
import os.path
import selectors
import socket
import time

import lirc.config
import _client

_DEFAULT_PROG = 'lircd-client'


def get_default_socket_path() -> str:
    ''' Get default value for the lircd socket path, using (falling priority):

       - The environment variable LIRC_SOCKET_PATH.
       - The 'output' value in the lirc_options.conf file if value and the
         corresponding file exists.
       - A hardcoded default lirc.config.VARRUNDIR/lirc/lircd, possibly
         non-existing.
    '''

    if 'LIRC_SOCKET_PATH' in os.environ:
        return os.environ['LIRC_SOCKET_PATH']
    path = lirc.config.SYSCONFDIR + '/lirc/lirc_options.conf'
    parser = configparser.SafeConfigParser()
    try:
        parser.read(path)
    except configparser.Error:
        pass
    else:
        if parser.has_section('lircd'):
            try:
                path = str(parser.get('lircd', 'output'))
                if os.path.exists(path):
                    return path
            except configparser.NoOptionError:
                pass
    return lirc.config.VARRUNDIR + '/lirc/lircd'


def get_default_lircrc_path() -> str:
    ''' Get default path to the lircrc file according to (falling priority):

       - $XDG_CONFIG_HOME/lircrc if environment variable and file exists.
       - ~/.config/lircrc if it exists.
       - ~/.lircrc if it exists
       - A hardcoded default lirc.config.SYSCONFDIR/lirc/lircrc, whether
         it exists or not.
    '''
    if 'XDG_CONFIG_HOME' in os.environ:
        path = os.path.join(os.environ['XDG_CONFIG_HOME'], 'lircrc')
        if os.path.exists(path):
            return path
    path = os.path.join(os.path.expanduser('~'), '.config' 'lircrc')
    if os.path.exists(path):
        return path
    path = os.path.join(os.path.expanduser('~'), '.lircrc')
    if os.path.exists(path):
        return path
    return os.path.join(lirc.config.SYSCONFDIR, 'lirc', 'lircrc')


class BadPacketException(Exception):
    ''' Malformed or otherwise unparsable packet received. '''
    pass


class TimeoutException(Exception):
    ''' Timeout receiving data from remote host.'''
    pass


##
#   @defgroup receiving Classes to receive keypresses
#
#   Interface to read raw code strings like irw(1) or
#   translated application strings like ircat(1).
#
#   Reading raw data
#   ----------------
#
#   Reading raw data direct from the lircd socket can be done with the
#   RawConnection object using something like
#
#          from lirc import RawConnnection
#          with RawConnection(socket_path) as conn:
#              while True:
#                  keypress = conn.readline()
#                  ... do something with keypress
#
#   The optional socket_path argument is the path to the lircd socket.
#   Refer to get_default_socket_path() for defaults if omitted.
#
#   See also the @ref irw.py example.
#
#
#   Reading lircrc-translated data.
#   -------------------------------
#
#   Reading application strings translated with lircrc can be achieved using
#
#          from lirc import LircdConnection
#          with LircdConnection(program, lircrc_path, socket_path) as conn:
#              while True:
#                  string = conn.readline()
#                  ... do domething with string
#
#   The arguments:
#       - program: Program identifier as described in ircat(1).
#       - lircrc_path: Path to lircrc  file. See
#         get_default_lircrc_path() for defaults if omitted.
#       - socket_path: See RawConnection above.
#
#   See also the @ref ircat.py example
#
#   @example irw.py
#   @example ircat.py
#
#   @addtogroup receiving
#   @{


class AbstractConnection(metaclass=ABCMeta):
    ''' Abstract interface for all connections. '''

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, traceback):
        self.close()

    @abstractmethod
    def readline(self, timeout: float = None) -> str:
        ''' Read a buffered line

        Parameters:
          - timeout: seconds.
              - If set to 0 immediately return either a line or None.
              - If set to None (default mode) use blocking read.

        Returns: code string as described in lircd(8) without trailing
                 newline or None.

        Raises: TimeoutException  if timeout > 0 expires.
        '''
        pass

    @abstractmethod
    def fileno(self) -> int:
        ''' Return the file nr used for IO, suitable for select() etc. '''
        pass

    @abstractmethod
    def has_data(self) -> bool:
        ''' Return true if next readline(None) won't block . '''
        pass

    @abstractmethod
    def close(self):
        ''' Close/release all resources '''
        pass


class RawConnection(AbstractConnection):
    ''' Interface to receive code strings as described in lircd(8).

    Parameters:
      - socket_path: lircd output socket path, see get_default_socket_path()
        for defaults.
      - prog: Program name used in lircrc decoding, see ircat(1). Could be
        omitted if only raw keypresses should be read.

    '''
    # pylint: disable=no-member

    def __init__(self, socket_path: str = None, prog: str = _DEFAULT_PROG):
        if socket_path:
            os.environ['LIRC_SOCKET_PATH'] = socket_path
        else:
            os.environ['LIRC_SOCKET_PATH'] = get_default_socket_path()
        _client.lirc_deinit()
        fd = _client.lirc_init(prog)
        self._socket = socket.fromfd(fd, socket.AF_UNIX, socket.SOCK_STREAM)
        self._select = selectors.DefaultSelector()
        self._select.register(self._socket, selectors.EVENT_READ)
        self._buffer = bytearray(0)

    def readline(self, timeout: float = None) -> str:
        ''' Implements AbstractConnection.readline(). '''
        if timeout:
            start = time.clock()
        while b'\n' not in self._buffer:
            ready = self._select.select(
                start + timeout - time.clock() if timeout else timeout)
            if ready == []:
                if timeout:
                    raise TimeoutException(
                        "readline: no data within %f seconds" % timeout)
                else:
                    return None
            recv = self._socket.recv(4096)
            if len(recv) == 0:
                raise ConnectionResetError('Connection lost')
            self._buffer += recv
        line, self._buffer = self._buffer.split(b'\n', 1)
        return line.decode('ascii', 'ignore')

    def fileno(self) -> int:
        ''' Implements AbstractConnection.fileno(). '''
        return self._socket.fileno()

    def has_data(self) -> bool:
        ''' Implements AbstractConnection.has_data() '''
        return b'\n' in self._buffer

    def close(self):
        ''' Implements AbstractConnection.close() '''
        self._socket.close()
        _client.lirc_deinit()


AbstractConnection.register(RawConnection)          # pylint:disable=no-member


class LircdConnection(AbstractConnection):
    ''' Interface to receive lircrc-translated keypresses. This is basically
    built on top of lirc_code2char() and as such supporting centralized
    translations using lircrc_class. See lircrcd(8).

    Parameters:
      - program: string, used to identify client. See ircat(1)
      - lircrc: lircrc file path. See get_default_lircrc_path() for defaults.
      - socket_path: lircd output socket path,  see get_default_socket_path()
        for defaults.
    '''
    # pylint: disable=no-member

    def __init__(self, program: str,
                 lircrc_path: str = None,
                 socket_path: str = None):
        if not lircrc_path:
            lircrc_path = get_default_lircrc_path()
        if not lircrc_path:
            raise FileNotFoundError('Cannot find lircrc config file.')
        self._connection = RawConnection(socket_path, program)
        self._lircrc = _client.lirc_readconfig(lircrc_path)
        self._program = program
        self._buffer = []

    def readline(self, timeout: float = None):
        ''' Implements AbstractConnection.readline(). '''
        while len(self._buffer) <= 0:
            code = self._connection.readline(timeout)
            if code is None:
                return None
            strings = \
                _client.lirc_code2char(self._lircrc, self._program, code)
            if not strings or len(strings) == 0:
                if timeout == 0:
                    return None
                continue
            self._buffer.extend(strings)
        return self._buffer.pop(0)

    def has_data(self) -> bool:
        ''' Implements AbstractConnection.has_data() '''
        return len(self._buffer) > 0

    def fileno(self) -> int:
        ''' Implements AbstractConnection.fileno(). '''
        return self._connection.fileno()

    def close(self):
        ''' Implements AbstractConnection.close() '''
        self._connection.close()
        _client.lirc_freeconfig(self._lircrc)


AbstractConnection.register(LircdConnection)     # pylint: disable=no-member

## @}


##
#   @defgroup sending Classes to send commands
#
#   Classes to send a Command to lircd and parse the reply.
#
#   Sending data
#   ------------
#
#   Sending commands is about creating a command and connection. In the
#   simplest form it looks like
#
#       import lirc
#       with lirc.CommandConnection(socket_path=...) as conn:
#           reply = lirc.StopRepeatCommand(conn, 'mceusb', 'KEY_1').run()
#       if not reply.success:
#           print(parser.data[0])
#
#   See also the @ref list-remotes.py, @ref list-keys.py and @ref simulate.py
#   examples.
#
#   The parameters depends on the actual command; there is a Command
#   defined for all known lircd commands. socket_path can often be omitted,
#   see get_default_socket_path() for default locations used.
#
#   The returned object is a Reply with various info on the processed
#   command.
#
#   To get more control lower-level primitives could be used instead of
#   run() as in this example:
#
#       while not command.parser.is_completed():
#           line = conn.readline(0.1)
#           if line:
#               command.parser.feed(line)
#           else:
#               ... handle timeout
#       if not command.parser.result == lirc.client.Result.OK:
#           print('Cannot get version string')
#       else:
#           print(command.parser.data[0])
#       ...
#       conn.close()
#
#       @example simulate.py
#       @example list-keys.py
#       @example list-remotes.py
#
#    @addtogroup sending
#    @{


class CommandConnection(RawConnection):
    ''' Extends the parent with a send() method. '''

    def __init__(self, socket_path: str = None):
        RawConnection.__init__(self, socket_path)

    def send(self, command: (bytearray, str)):
        ''' Send  single line over socket '''
        if not isinstance(command, bytearray):
            command = command.encode('ascii')
        while len(command) > 0:
            sent = self._socket.send(command)
            command = command[sent:]


class Result(Enum):
    ''' Public reply parser result, available when completed. '''
    OK = 1
    FAIL = 2
    INCOMPLETE = 3


class Command(object):
    ''' Command, parser and connection container with a run() method. '''

    def __init__(self, cmd: str,
                 connection: AbstractConnection,
                 timeout: float = 0.4):
        self._conn = connection
        self._cmd_string = cmd
        self._parser = ReplyParser()

    def run(self, timeout: float = None):
        ''' Run the command and return a Reply. Timeout as of
        AbstractConnection.readline()
        '''
        self._conn.send(self._cmd_string)
        while not self._parser.is_completed():
            line = self._conn.readline(timeout)
            if not line:
                raise TimeoutException('No data from lircd host.')
            self._parser.feed(line)
        return self._parser


class Reply(object):
    ''' The status/result from parsing a command reply.

    Attributes:
        result: Enum Result, reflects parser state.
        success: bool, reflects SUCCESS/ERROR.
        data: List of lines, the command DATA payload.
        sighup: bool, reflects if a SIGHUP package has been received
                (these are otherwise ignored).
        last_line: str, last input line (for error messages).
    '''
    def __init__(self):
        self.result = Result.INCOMPLETE
        self.success = None
        self.data = []
        self.sighup = False
        self.last_line = ''


class ReplyParser(Reply):
    ''' Handles the actual parsing of a command reply.  '''

    def __init__(self):
        Reply.__init__(self)
        self._state = self._State.BEGIN
        self._lines_expected = None
        self._buffer = bytearray(0)

    def is_completed(self) -> bool:
        ''' Returns true if no more reply input is required. '''
        return self.result != Result.INCOMPLETE

    def feed(self, line: str):
        ''' Enter a line of data into parsing FSM, update state. '''

        fsm = {
            self._State.BEGIN: self._begin,
            self._State.COMMAND: self._command,
            self._State.RESULT: self._result,
            self._State.DATA: self._data,
            self._State.LINE_COUNT: self._line_count,
            self._State.LINES: self._lines,
            self._State.END: self._end,
            self._State.SIGHUP_END: self._sighup_end
        }
        line = line.strip()
        if not line:
            return
        self.last_line = line
        fsm[self._state](line)
        if self._state == self._State.DONE:
            self.result = Result.OK

##
#  @defgroup FSM Internal parser FSM
#  @{
#  Internal parser FSM.
#  pylint: disable=missing-docstring,redefined-variable-type

    class _State(Enum):
        ''' Internal FSM state. '''
        BEGIN = 1
        COMMAND = 2
        RESULT = 3
        DATA = 4
        LINE_COUNT = 5
        LINES = 6
        END = 7
        DONE = 8
        NO_DATA = 9
        SIGHUP_END = 10

    def _bad_packet_exception(self, line):
        self.result = Result.FAIL
        raise BadPacketException(
            'Cannot parse: %s\nat state: %s\n' % (line, self._state))

    def _begin(self, line):
        if line == 'BEGIN':
            self._state = self._State.COMMAND

    def _command(self, line):
        if not line:
            self._bad_packet_exception(line)
        elif line == 'SIGHUP':
            self._state = self._State.SIGHUP_END
            self.sighup = True
        else:
            self._state = self._State.RESULT

    def _result(self, line):
        if line in ['SUCCESS', 'ERROR']:
            self.success = line == 'SUCCESS'
            self._state = self._State.DATA
        else:
            self._bad_packet_exception(line)

    def _data(self, line):
        if line == 'END':
            self._state = self._State.DONE
        elif line == 'DATA':
            self._state = self._State.LINE_COUNT
        else:
            self._bad_packet_exception(line)

    def _line_count(self, line):
        try:
            self._lines_expected = int(line)
        except ValueError:
            self._bad_packet_exception(line)
        if self._lines_expected == 0:
            self._state = self._State.END
        else:
            self._state = self._State.LINES

    def _lines(self, line):
        self.data.append(line)
        if len(self.data) >= self._lines_expected:
            self._state = self._State.END

    def _end(self, line):
        if line != 'END':
            self._bad_packet_exception(line)
        self._state = self._State.DONE

    def _sighup_end(self, line):
        if line == 'END':
            ReplyParser.__init__(self)
            self.sighup = True
        else:
            self._bad_packet_exception(line)

## @}
#  FSM
#  pylint: enable=missing-docstring,redefined-variable-type

## @}


##   @defgroup commands Commands to control lircd
#    Canned classes, one for each command in the lircd(8) socket
#    interface.
#
#    @addtogroup commands
#    @{


class SimulateCommand(Command):
    ''' Simulate a button press, see SIMULATE in lircd(8) manpage.  '''
    # pylint: disable=too-many-arguments

    def __init__(self, connection: AbstractConnection,
                 remote: str, key: str, repeat: int = 1, keycode: int = 0):
        cmd = 'SIMULATE %016d %02d %s %s\n' % \
            (int(keycode), int(repeat), key, remote)
        Command.__init__(self, cmd, connection)


class ListRemotesCommand(Command):
    ''' List available remotes, see LIST in lircd(8) manpage. '''

    def __init__(self, connection: AbstractConnection):
        Command.__init__(self, 'LIST\n', connection)


class ListKeysCommand(Command):
    ''' List available keys in given remote, see LIST in lircd(8) manpage. '''

    def __init__(self, connection: AbstractConnection, remote: str):
        Command.__init__(self, 'LIST %s\n' % remote, connection)


class StartRepeatCommand(Command):
    ''' Start repeating given key, see SEND_START in lircd(8) manpage. '''

    def __init__(self, connection: AbstractConnection,
                 remote: str, key: str):
        cmd = 'SEND_START %s %s\n' % (remote, key)
        Command.__init__(self, cmd, connection)


class StopRepeatCommand(Command):
    ''' Stop repeating given key, see SEND_STOP in lircd(8) manpage. '''

    def __init__(self, connection: AbstractConnection,
                 remote: str, key: str):
        cmd = 'SEND_STOP %s %s\n' % (remote, key)
        Command.__init__(self, cmd, connection)


class SendCommand(Command):
    ''' Send given key, see SEND_ONCE in lircd(8) manpage. '''

    def __init__(self, connection: AbstractConnection,
                 remote: str, keys: str):
        if not len(keys):
            raise ValueError('No keys to send given')
        cmd = 'SEND_ONCE %s %s\n' % (remote, ' '.join(keys))
        Command.__init__(self, cmd, connection)


class SetTransmittersCommand(Command):
    ''' Set transmitters to use, see SET_TRANSMITTERS in lircd(8) manpage.

    Arguments:
        transmitter: Either a bitmask or a list of int describing active
            transmitter numbers.
    '''

    def __init__(self, connection: AbstractConnection,
                 transmitters: (int, list)):
        if isinstance(transmitters, list):
            mask = 0
            for transmitter in transmitters:
                mask |= (1 << (int(transmitter) - 1))
        else:
            mask = transmitters
        cmd = 'SET_TRANSMITTERS %d\n' % mask
        Command.__init__(self, cmd, connection)


class VersionCommand(Command):
    ''' Get lircd version, see VERSION in lircd(8) manpage. '''

    def __init__(self, connection: AbstractConnection):
        Command.__init__(self, 'VERSION\n', connection)


class DrvOptionCommand(Command):
    ''' Set a driver option value, see DRV_OPTION in lircd(8) manpage. '''

    def __init__(self, connection: AbstractConnection,
                 option: str, value: str):
        cmd = 'DRV_OPTION %s %s\n' % (option, value)
        Command.__init__(self, cmd, connection)


class SetLogCommand(Command):
    ''' Start/stop logging lircd output , see SET_INPUTLOG in lircd(8)
    manpage.
    '''

    def __init__(self, connection: AbstractConnection,
                 logfile: str = None):
        cmd = 'SET_INPUTLOG' + (' ' + logfile if logfile else '') + '\n'
        Command.__init__(self, cmd, connection)

##  @}


##   @defgroup lircrcd Commands to control lircrcd
#    Canned classes, one for each command in the lircrcd(l8) socket
#    interface.
#
#    @addtogroup lircrcd
#    @{


class IdentCommand(Command):
    ''' Identify client using the prog token, see IDENT in lircrcd(8) '''

    def __init__(self, connection: AbstractConnection,
                 prog: str = None):
        if not prog:
            raise ValueError('The prog argument cannot be None')
        cmd = 'IDENT {}\n'.format(prog)
        Command.__init__(self, cmd, connection)


class CodeCommand(Command):
    '''Translate a keypress to application string, see CODE in lircrcd(8) '''

    def __init__(self, connection: AbstractConnection,
                 code: str = None):
        if not code:
            raise ValueError('The prog argument cannot be None')
        Command.__init__(self, 'CODE {}\n'.format(code), connection)


class GetModeCommand(Command):
    '''Get current translation mode, see GETMODE in lircrcd(8) '''

    def __init__(self, connection: AbstractConnection):
        Command.__init__(self, "GETMODE\n", connection)


class SetModeCommand(Command):
    '''Set current translation mode, see SETMODE in lircrcd(8) '''

    def __init__(self, connection: AbstractConnection,
                 mode: str = None):
        if not mode:
            raise ValueError('The mode argument cannot be None')
        Command.__init__(self, 'SETMODE {}\n'.format(mode), connection)

##  @}


##  @}
#   python-bindings
