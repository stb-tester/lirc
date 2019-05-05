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

import signal
from contextlib import contextmanager, suppress
from concurrent.futures import TimeoutError

_PACKET_ONE = '0123456789abcdef 00 KEY_1 mceusb'
_LINE_0 = '0123456789abcdef 00 KEY_1 mceusb'
_SOCKET = 'lircd.socket'
_SOCAT = subprocess.check_output('which socat', shell=True) \
    .decode('ascii').strip()
_EXPECT = subprocess.check_output('which expect', shell=True) \
    .decode('ascii').strip()


class TimeoutException(Exception):
    pass

@contextmanager
def event_loop(suppress=[]):

    if isinstance(suppress, type) and issubclass(suppress, Exception):
        suppress = [suppress]
    elif isinstance(suppress, list):
        for ex_type in suppress:
            if not isinstance(ex_type, type) or not issubclass(ex_type, Exception):
                raise ValueError('suppress is not an array of exception types')
    else:
        raise ValueError('suppress is not an exception type')

    ex = []
    def exception_handler(loop, context):
        nonlocal ex
        nonlocal suppress
        for ex_type in suppress:
            if isinstance(context['exception'], ex_type):
                return
        ex.append(context['exception'])

    loop = asyncio.get_event_loop()
    if loop.is_closed():
        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)
    loop.set_exception_handler(exception_handler)

    try:
        yield loop
    finally:
        loop.close()
        if len(ex):
            raise Exception('Unhandled exceptions in async code') from ex[0]

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

    @contextmanager
    def assertCompletedBeforeTimeout(self, timeout):

        triggered = False

        def handle_timeout(signum, frame):
            nonlocal triggered
            triggered = True
            for task in asyncio.Task.all_tasks():
                task.cancel()
            raise TimeoutException()

        try:
            signal.signal(signal.SIGALRM, handle_timeout)
            signal.alarm(timeout)
            with suppress(TimeoutException, asyncio.CancelledError):
                yield
        finally:
            signal.signal(signal.SIGALRM, signal.SIG_DFL)
            signal.alarm(0)
            if triggered:
                raise self.failureException('Code block did not complete before the {} seconds timeout'.format(timeout)) from None

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

        async def get_lines(raw_conn, count, loop):

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
            with LircdConnection('foo',
                                 socket_path=_SOCKET,
                                 lircrc_path='lircrc.conf') as conn:
                with event_loop() as loop:
                    loop.run_until_complete(get_lines(conn, 1000, loop))

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

    def testReceiveDisconnect(self):
        ''' Generate a ConnectionResetError if connection is lost '''

        if os.path.exists(_SOCKET):
            os.unlink(_SOCKET)
        cmd = [_SOCAT, 'UNIX-LISTEN:' + _SOCKET, 'EXEC:"sleep 1"']
        with subprocess.Popen(cmd) as child:
            _wait_for_socket()
            with LircdConnection('foo',
                                 socket_path=_SOCKET,
                                 lircrc_path='lircrc.conf') as conn:
                with self.assertRaises(ConnectionResetError):
                    with self.assertCompletedBeforeTimeout(3):
                        conn.readline(2)

    def testReceiveAsyncDisconnectDontBlock(self):
        ''' Do not block the loop if connection is lost '''

        async def readline(raw_conn):
            async with AsyncConnection(raw_conn, loop) as conn:
                return await conn.readline()

        if os.path.exists(_SOCKET):
            os.unlink(_SOCKET)
        cmd = [_SOCAT, 'UNIX-LISTEN:' + _SOCKET, 'EXEC:"sleep 1"']
        with subprocess.Popen(cmd) as child:
            _wait_for_socket()
            with LircdConnection('foo',
                                 socket_path=_SOCKET,
                                 lircrc_path='lircrc.conf') as conn:
                with event_loop(suppress=[ConnectionResetError, TimeoutException]) as loop:
                    with self.assertCompletedBeforeTimeout(3):
                        with suppress(TimeoutError, ConnectionResetError):
                            loop.run_until_complete(asyncio.wait_for(readline(conn), 2))

    def testReceiveAsyncExceptionReraises(self):
        ''' Async readline should reraise if an exception occurs during select loop '''

        async def readline(raw_conn):
            async with AsyncConnection(raw_conn, loop) as conn:
                return await conn.readline()

        if os.path.exists(_SOCKET):
            os.unlink(_SOCKET)
        cmd = [_SOCAT, 'UNIX-LISTEN:' + _SOCKET, 'EXEC:"sleep 1"']
        with subprocess.Popen(cmd) as child:
            _wait_for_socket()
            with LircdConnection('foo',
                                 socket_path=_SOCKET,
                                 lircrc_path='lircrc.conf') as conn:
                with event_loop(suppress=[ConnectionResetError, TimeoutException]) as loop:
                    with self.assertCompletedBeforeTimeout(2):
                        with self.assertRaises(ConnectionResetError):
                            loop.run_until_complete(readline(conn))

    def testReceiveAsyncExceptionEndsIterator(self):
        ''' Async iterator should stop if an exception occurs in the select loop '''

        async def get_lines(raw_conn):
            async with AsyncConnection(raw_conn, loop) as conn:
                async for keypress in conn:
                    pass

        if os.path.exists(_SOCKET):
            os.unlink(_SOCKET)
        cmd = [_SOCAT, 'UNIX-LISTEN:' + _SOCKET, 'EXEC:"sleep 1"']
        with subprocess.Popen(cmd) as child:
            _wait_for_socket()
            with LircdConnection('foo',
                                 socket_path=_SOCKET,
                                 lircrc_path='lircrc.conf') as conn:
                with event_loop(suppress=[ConnectionResetError, TimeoutException]) as loop:
                    with self.assertCompletedBeforeTimeout(2):
                        self.assertIsNone(loop.run_until_complete(get_lines(conn)))

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
