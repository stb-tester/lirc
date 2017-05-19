'''

Do sommethind like "irsend simulate". Run using
LIRC_SOCKET_PATH=... python3 simulate.py <remote> <key> [repeat [code]]

'''

import sys

import lirc

if len(sys.argv) < 3 or len(sys.argv) > 5:
    sys.stderr.write("Usage: simulate.py  <remote> <key> [repeat [code ]]")
    sys.exit(1)
code = sys.argv[4] if len(sys.argv) >= 5 else 9
repeat = sys.argv[3] if len(sys.argv) >= 4 else 1
key = sys.argv[2]
remote = sys.argv[1]

with lirc.CommandConnection() as conn:
    reply = lirc.SimulateCommand(conn, remote, key, repeat, code).run()
if not reply.success:
    print(reply.data[0])

# conn.send(command.cmd_string)
# while not command.parser.is_completed():
#     line = conn.readline(0.1)
#     command.parser.feed(line)
# if not command.parser.result == lirc.client.Result.OK:
#     print("Cannot get version string")
# else:
#     print(command.parser.data[0])
