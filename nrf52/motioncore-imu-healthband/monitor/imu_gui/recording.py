"""CSV recording and fixed-size history buffers for the live charts."""

#
# ------------------------------------------------------------------
#  Request Full Access to Author: Faizan Saleem Siddiqui
#  Email: faizansaleemsdq@yahoo.com
#  (73 lines of this file are hidden by the author)
# ------------------------------------------------------------------
#
        block = self._data[:, idx]
        return block[0], {name: block[i + 1] for i, name in enumerate(self.names)}
