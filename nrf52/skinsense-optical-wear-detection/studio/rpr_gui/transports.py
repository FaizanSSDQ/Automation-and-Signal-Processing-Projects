"""
Connections to the firmware: BLE, serial (UART / J-Link VCOM) and a simulator.
#
# ------------------------------------------------------------------
#  Request Full Access to Author: Faizan Saleem Siddiqui
#  Email: faizansaleemsdq@yahoo.com
#  (366 lines of this file are hidden by the author)
# ------------------------------------------------------------------
#
    """Schedule a coroutine on the running (qasync) event loop."""
    return asyncio.ensure_future(coro)
