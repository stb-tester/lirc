''' Asynchronous python bindings for the lircd socket interface. '''
##
#   @file async_client.py
#   @author Alec Leamas
#   @brief Asynchronour python bindings for a subset of the lirc_client.h
#   interface.
#   @ingroup  python_bindings

##  @addtogroup receiving
#   @{
#
#   Asynchronous interfaces to read lirc data on tóp of client.py.
#
#
#   Asynchronous read
#   -----------------
#
#   Asynchronous read of raw data direct from the lircd socket can be
#   done with the RawConnection class using something like:
#
#          import asyncio
#          from lirc import RawConnection, AsyncConnection
#
#          async def main(raw_conn, loop):
#              async with AsyncConnection(raw_conn, loop) as conn:
#                  async for keypress in conn:
#                      print(keypress)
#
#          if __name__ == "__main__":
#              socket_path =  .....
#              loop = asyncio.get_event_loop()
#              with RawConnection(socket_path) as raw_conn:
#                  loop.run_until_complete(main(raw_conn, loop))
#              loop.close()
#
#
#   Using a LircdConnection with translated values works the same way.
#   The API is unstable.

#   pylint: disable=W0613

import asyncio
from lirc.client import AbstractConnection as AbstractConnection


class AsyncConnection(object):
    ''' Asynchronous read interface on top of an AbstractConnection.

    Parameters:
       - connection: Typically a lirc.RawConnection or lirc.LircdConnection.
       - loop: AbstractEventLoop, typically obtained using
               asyncio.get_event_loop().
    '''

    def __init__(self, connection: AbstractConnection,
                 loop: asyncio.AbstractEventLoop):

        def read_from_fd():
            ''' Read data from the connection fd and put into queue. '''
            try:
                line = self._conn.readline(0)
                if line:
                    asyncio.ensure_future(self._queue.put(line))
            except Exception as e:
                self.close()
                self._queue.put_nowait(e)

        self._conn = connection
        self._loop = loop
        self._queue = asyncio.Queue(loop=self._loop)
        self._loop.add_reader(self._conn.fileno(), read_from_fd)

    def close(self):
        ''' Clean up loop and the base connection. '''
        self._loop.remove_reader(self._conn.fileno())

    async def readline(self) -> str:
        ''' Asynchronous get next line from the connection. '''
        line = await self._queue.get()
        if isinstance(line, Exception):
            raise line
        return line

    def __aiter__(self):
        ''' Return async iterator. '''
        return self

    async def __anext__(self):
        ''' Implement async iterator.next(). '''
        line = await self._queue.get()
        if isinstance(line, Exception):
            raise StopAsyncIteration
        return line

    async def __aenter__(self):
        ''' Implement "async with". '''
        return self

    async def __aexit__(self, exc_type, exc, traceback):
        ''' Implement exit from "async with". '''
        self.close()

## @}
