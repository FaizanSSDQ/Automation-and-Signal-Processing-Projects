"""Parsing of the BMI270 firmware's UART stream.

#
# ------------------------------------------------------------------
#  Request Full Access to Author: Faizan Saleem Siddiqui
#  Email: faizansaleemsdq@yahoo.com
#  (103 lines of this file are hidden by the author)
# ------------------------------------------------------------------
#
        tilt = math.degrees(math.acos(max(-1.0, min(1.0, az / amag)))) if amag > 0 else 0.0
        return ImuSample(seq, t_us / 1e6, ax, ay, az, gx / kg, gy / kg, gz / kg, amag, roll, pitch, tilt)
