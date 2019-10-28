# Run with: pytest-3 -vv test/test_release_mask.py

import os
import subprocess
import time
from textwrap import dedent

import pytest


class Lircd(object):
    def __init__(self, socket, output):
        self.socket = socket
        self.output = output

    def irsend(self, *args, count=None):
        if count is not None:
            count_args = ["--count=%s" % count]
        else:
            count_args = []
        subprocess.check_call(
            [_find_file("../tools/irsend"),
             "--device", self.socket,
            ] + count_args + list(args))


@pytest.fixture(scope="function")
def lircd(tmpdir):
    socket = tmpdir.join("lircd.sock").strpath
    output = tmpdir.join("driver.out").strpath

    p = subprocess.Popen(
        [_find_file("../daemons/lircd"),
         "--nodaemon",
         "--plugindir", _find_file("../plugins/.libs"),
         "--pidfile", tmpdir.join("lircd.pid").strpath,
         "--driver=file",
         "--device", output,
         "--output", socket,
         _find_file("test_release_mask.lircd.conf")])
    for _ in range(10):
        if p.poll() is not None or os.path.exists(socket):
            break
        time.sleep(0.1)
    else:
        assert False, "lircd failed to start up within 1s"
    assert p.returncode is None, "lircd exited at startup"
    print("Found %s" % socket)
    try:
        yield Lircd(socket, output)
    finally:
        p.terminate()
        p.wait()


@pytest.mark.parametrize(
    "min_repeats_in_config,irsend_count,expected_signals",
    [(0, None, 1),
     (0, 1, 1),
     (0, 2, 3),
     (0, 5, 6),
     (1, None, 2),
     (1, 1, 2),
     (1, 2, 3),
     (1, 5, 6),
     (2, None, 3),
     (2, 1, 3),
     (2, 2, 3),
     (2, 5, 6),
    ])
@pytest.mark.parametrize("has_release_mask", [1, 0])
def test_release_mask(lircd, has_release_mask, min_repeats_in_config,
                      irsend_count, expected_signals):

    if has_release_mask:
        remote = "has_release_%i_repeats" % min_repeats_in_config
    else:
        remote = "no_release_%i_repeats" % min_repeats_in_config
    lircd.irsend("SEND_ONCE", remote, "KEY_1", count=irsend_count)
    expected = (single_signal * expected_signals +
                release_signal * has_release_mask)
    actual = open(lircd.output).read()
    assert expected_signals + has_release_mask == sum(
        1 for line in actual.split("\n") if line == "space 90000")
    assert expected == actual


def _find_file(f, root=os.path.dirname(__file__)):
    return os.path.join(root, f)


single_signal = dedent("""\
    pulse 417
    space 278
    pulse 167
    space 278
    pulse 167
    space 778
    pulse 167
    space 278
    pulse 167
    space 278
    pulse 167
    space 278
    pulse 167
    space 278
    pulse 167
    space 278
    pulse 167
    space 278
    pulse 167
    space 278
    pulse 167
    space 611
    pulse 167
    space 444
    pulse 167
    space 611
    pulse 167
    space 278
    pulse 167
    space 278
    pulse 167
    space 278
    pulse 167
    space 444
    pulse 167
    space 90000
    """)

release_signal = "\n".join(
    single_signal.split("\n")[:19] +
    ["space 611"] +
    single_signal.split("\n")[20:])
