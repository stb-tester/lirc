'''

Get list of available remotes from lircd server. Run using
python3 list-remotes.py <remote> [socket_path]

'''

import sys

import lirc

if len(sys.argv) >= 3 or len(sys.argv) < 2:
    sys.stderr.write("Usage: list-keys.py <remote> [socket path]")
    sys.exit(1)
path = sys.argv[2] if len(sys.argv) == 3 else None
remote = sys.argv[1]

with lirc.CommandConnection(path) as conn:
    reply = lirc.ListKeysCommand(conn, remote).run()
for key in reply.data:
    print(key)

# conn.send(command.cmd_string)
# while not command.parser.is_completed():
#     line = conn.readline(0.1)
#     command.parser.feed(line)
# if not command.parser.result == lirc.client.Result.OK:
#     print("Cannot get version string")
# else:
#     print(command.parser.data[0])
