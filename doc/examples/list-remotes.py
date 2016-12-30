'''

Get list of available remotes from lircd server. Run using
LIRC_SOCKET_PATH=... python3 list-remotes.py

'''

import sys

import lirc

if len(sys.argv) >= 2:
    sys.stderr.write("Usage: list-remotes.py [socket path]")
    sys.exit(1)
path = sys.argv[1] if len(sys.argv) == 2 else None

with lirc.client.CommandConnection(path) as conn:
    reply = lirc.client.ListRemotesCommand(conn).run()
print(reply.data[0])

# conn.send(command.cmd_string)
# while not command.parser.is_completed():
#     line = conn.readline(0.1)
#     command.parser.feed(line)
# if not command.parser.result == lirc.client.Result.OK:
#     print("Cannot get version string")
# else:
#     print(command.parser.data[0])
