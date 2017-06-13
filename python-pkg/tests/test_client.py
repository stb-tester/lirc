''' Test receiving, primarely RawConnection and LircdConnnection. '''

import asyncio
import os
import os.path
import subprocess
import sys
import time
import unittest

testdir = os.path.abspath(os.path.dirname(__file__))
os.chdir(testdir)

sys.path.insert(0, os.path.abspath(os.path.join(testdir, '..')))

from lirc import RawConnection, LircdConnection, CommandConnection
from lirc import AsyncConnection
import lirc

_PACKET_ONE = '0123456789abcdef 00 KEY_1 mceusb'
_LINE_0 = '0123456789abcdef 00 KEY_1 mceusb'
_SOCKET = 'lircd.socket'
_SOCAT = subprocess.check_output('which socat', shell=True) \
    .decode('ascii').strip()
_EXPECT = subprocess.check_output('which expect', shell=True) \
    .decode('ascii').strip()


def _wait_for_socket():
    ''' Wait until the ncat process has setup the lircd.socket dummy. '''
    i = 0
    while not os.path.exists(_SOCKET):
        time.sleep(0.01)
        i += 1
        if i > 100:
            raise OSError('Cannot find socket file')


class ReceiveTests(unittest.TestCase):
    ''' Test various Connections. '''

    def testReceiveOneRawLine(self):
        ''' Receive a single, raw line. '''

        if os.path.exists(_SOCKET):
            os.unlink(_SOCKET)
        cmd = [_SOCAT,  'UNIX-LISTEN:' + _SOCKET,
               'EXEC:"echo %s"' % _PACKET_ONE]
        with subprocess.Popen(cmd) as child:
            _wait_for_socket()
            with RawConnection(socket_path=_SOCKET) as conn:
                line = conn.readline()
            self.assertEqual(line, _PACKET_ONE)

    def testReceive10000RawLines(self):
        ''' Receive 10000 raw lines. '''

        if os.path.exists(_SOCKET):
            os.unlink(_SOCKET)
        cmd = [_SOCAT, 'UNIX-LISTEN:' + _SOCKET,
                'EXEC:"%s ./dummy-server 0"' % _EXPECT]
        with subprocess.Popen(cmd,
                              stdout = subprocess.PIPE,
                              stderr = subprocess.STDOUT) as child:
            _wait_for_socket()
            lines = []
            with RawConnection(socket_path=_SOCKET) as conn:
                for i in range(0, 10000):
                    lines.append(conn.readline())
            self.assertEqual(lines[0], _LINE_0)
            self.assertEqual(lines[9999], _LINE_0.replace(" 00 ", " 09 "))

    def testReceiveOneLine(self):
        ''' Receive a single, translated line OK. '''

        if os.path.exists(_SOCKET):
            os.unlink(_SOCKET)
        cmd = [_SOCAT,  'UNIX-LISTEN:' + _SOCKET,
               'EXEC:"echo %s"' % _PACKET_ONE]
        with subprocess.Popen(cmd) as child:
            _wait_for_socket()
            with LircdConnection('foo',
                                 socket_path=_SOCKET,
                                 lircrc_path='lircrc.conf') as conn:
                line = conn.readline()
        self.assertEqual(line, 'foo-cmd')

    def testReceive1AsyncLines(self):
        ''' Receive 1000 lines using the async interface. '''

        async def get_lines(raw_conn, count):

            nonlocal lines
            async with AsyncConnection(raw_conn, loop) as conn:
                async for keypress in conn:
                    lines.append(keypress)
                    if len(lines) >= count:
                        return lines

        if os.path.exists(_SOCKET):
            os.unlink(_SOCKET)
        cmd = [_SOCAT, 'UNIX-LISTEN:' + _SOCKET,
               'EXEC:"%s ./dummy-server 0"' % _EXPECT]
        lines = []
        with subprocess.Popen(cmd,
                              stdout = subprocess.PIPE,
                              stderr = subprocess.STDOUT) as child:
            _wait_for_socket()
            loop = asyncio.get_event_loop()
            with LircdConnection('foo',
                                 socket_path=_SOCKET,
                                 lircrc_path='lircrc.conf') as conn:
                loop.run_until_complete(get_lines(conn, 1000))
            loop.close()

        self.assertEqual(len(lines), 1000)
        self.assertEqual(lines[0], 'foo-cmd')
        self.assertEqual(lines[999], 'foo-cmd')

    def testReceiveTimeout(self):
        ''' Generate a TimeoutException if there is no data '''

        if os.path.exists(_SOCKET):
            os.unlink(_SOCKET)
        cmd = [_SOCAT, 'UNIX-LISTEN:' + _SOCKET, 'EXEC:"sleep 1"']
        with subprocess.Popen(cmd) as child:
            _wait_for_socket()
            with LircdConnection('foo',
                                 socket_path=_SOCKET,
                                 lircrc_path='lircrc.conf') as conn:
                self.assertRaises(lirc.TimeoutException, conn.readline, 0.1)


class CommandTests(unittest.TestCase):
    ''' Test Command, Reply, ReplyParser and some Commands samples. '''

    def testRemotesCommmand(self):
        ''' Do LIST without arguments . '''

        if os.path.exists(_SOCKET):
            os.unlink(_SOCKET)
        cmd = [_SOCAT, 'UNIX-LISTEN:' + _SOCKET,
               'EXEC:"%s ./dummy-server 100"' % _EXPECT]
        with subprocess.Popen(cmd,
                              stdout = subprocess.PIPE,
                              stderr = subprocess.STDOUT) as child:
            _wait_for_socket()
            with CommandConnection(socket_path=_SOCKET) as conn:
                reply = lirc.ListRemotesCommand(conn).run()
            self.assertEqual(len(reply.data), 2)
            self.assertEqual(reply.success, True)
            self.assertEqual(reply.data[0], 'mceusb1')
            self.assertEqual(reply.data[1], 'mceusb2')
            self.assertEqual(reply.sighup, False)

    def testSighupReply(self):
        ''' Handle an unexpected SIGHUP in SEND_STOP reply. '''

        if os.path.exists(_SOCKET):
            os.unlink(_SOCKET)
        cmd = [_SOCAT, 'UNIX-LISTEN:' + _SOCKET,
               'EXEC:"%s ./dummy-server 100"' % _EXPECT]
        with subprocess.Popen(cmd,
                              stdout = subprocess.PIPE,
                              stderr = subprocess.STDOUT) as child:
            _wait_for_socket()
            with CommandConnection(socket_path=_SOCKET) as conn:
                reply = lirc.StopRepeatCommand(conn, 'mceusb', 'KEY_1').run()
            self.assertEqual(len(reply.data), 0)
            self.assertEqual(reply.success, True)
            self.assertEqual(reply.sighup, True)


if __name__ == '__main__':
    unittest.main()
