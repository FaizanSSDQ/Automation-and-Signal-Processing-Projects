"""Small custom widgets: stat cards, RGB LED mimic, PS level bar."""

#
# ------------------------------------------------------------------
#  Request Full Access to Author: Faizan Saleem Siddiqui
#  Email: faizansaleemsdq@yahoo.com
#  (161 lines of this file are hidden by the author)
# ------------------------------------------------------------------
#
            p.drawText(bar.adjusted(10, 0, -10, 0), Qt.AlignVCenter | Qt.AlignRight,
                       f"{self._value} / {self._max}")
