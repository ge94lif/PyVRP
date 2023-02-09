import pathlib
import time
from functools import lru_cache

from pyvrp.read import read as _read


@lru_cache
def read(where: str):
    """
    Lightweight wrapper around ``pyvrp.read()``, reading files relative to the
    current directory.
    """
    this_dir = pathlib.Path(__file__).parent
    return _read(this_dir / where)


def sleep(duration, get_now=time.perf_counter):
    """
    Custom sleep function. Built-in ``time.sleep()`` is not precise/depends on
    the OS, see https://stackoverflow.com/q/1133857/4316405.
    """
    now = get_now()
    end = now + duration
    while now < end:
        now = get_now()


class DummyTarget:
    """
    Dummy optimisation target that implements the OptimisationTarget protocol.
    """

    def __init__(self, cost: float):
        self._cost = cost

    def cost(self):
        return self._cost