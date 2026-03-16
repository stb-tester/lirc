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

    def read_output(self):
        with open(self.output, encoding="utf-8") as f:
            return [
                line for line in f.read().splitlines() if not line.startswith("#")]


@pytest.fixture(scope="function")
def lircd(tmpdir):
    socket = tmpdir.join("lircd.sock").strpath
    output = tmpdir.join("driver.out").strpath

    p = subprocess.Popen(
        [_find_file("../daemons/lircd"),
         "--nodaemon",
         "--loglevel=trace",
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
def test_release_mask_send_once(
        lircd: Lircd,
        has_release_mask: int,
        min_repeats_in_config: int,
        irsend_count: "int | None",
        expected_signals: int,
):
    if has_release_mask:
        remote = "has_release_%i_repeats" % min_repeats_in_config
    else:
        remote = "no_release_%i_repeats" % min_repeats_in_config
    lircd.irsend("SEND_ONCE", remote, "KEY_1", count=irsend_count)
    expected = (SIGNAL * expected_signals +
                SIGNAL_WITH_TOGGLED_MASK * has_release_mask)
    actual = lircd.read_output()
    assert expected_signals + has_release_mask == sum(
        1 for line in actual if line == "space 90000")
    assert expected == actual


@pytest.mark.parametrize(
    "min_repeats_in_config,sleep,expected_signals",
    [(0, 0, 1),
     (0, 0.1, 2),
     (0, 0.2, 3),
     (1, 0, 2),
     (1, 0.1, 2),
     (1, 0.2, 3),
     (5, 0, 6),
    ])
@pytest.mark.parametrize("has_release_mask", [1, 0])
def test_release_mask_send_start(
        lircd: Lircd,
        min_repeats_in_config: int,
        sleep: float,
        expected_signals: int,
        has_release_mask: int,
):
    if has_release_mask:
        remote = "has_release_%i_repeats" % min_repeats_in_config
    else:
        remote = "no_release_%i_repeats" % min_repeats_in_config
    expires = time.time() + 0.1 * (expected_signals + has_release_mask)
    lircd.irsend("SEND_START", remote, "KEY_1")
    time.sleep(sleep)
    lircd.irsend("SEND_STOP", remote, "KEY_1")
    time.sleep(expires - time.time())  # wait for lirc to finish sending
    expected = (SIGNAL * expected_signals +
                SIGNAL_WITH_TOGGLED_MASK * has_release_mask)
    actual = lircd.read_output()
    assert expected_signals + has_release_mask == sum(
        1 for line in actual if line == "space 90000")
    assert expected == actual


def _find_file(f, root=os.path.dirname(__file__)):
    return os.path.join(root, f)


SIGNAL = dedent("""\
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
    """).splitlines()

# toggle_bit_mask/release_mask 0x00008000 flips bit 15 of code 0x30002601,
# changing dibit 8 from 00 (space 278) to 10 (space 611).
SIGNAL_WITH_TOGGLED_MASK = (SIGNAL[:19] + ["space 611"] + SIGNAL[20:])


def test_toggle_bit_mask_rcmm(lircd: Lircd):
    """toggle_bit_mask must affect the transmitted RCMM signal.

    toggle_bit_mask_state starts at 0 and is XOR'd with toggle_bit_mask
    before each SEND_ONCE, so the first send has state=mask (toggled) and
    the second send has state=0 (original).
    """
    lircd.irsend("SEND_ONCE", "has_toggle_bit_mask", "KEY_1")
    lircd.irsend("SEND_ONCE", "has_toggle_bit_mask", "KEY_1")
    with open(lircd.output) as f:
        actual = "".join(
            line for line in f if not line.startswith("#"))
    expected = SIGNAL_WITH_TOGGLED_MASK + SIGNAL
    assert expected == actual
