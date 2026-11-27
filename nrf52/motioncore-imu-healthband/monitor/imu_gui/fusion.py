"""Orientation math: quaternions, Euler angles and a Mahony AHRS filter.

#
# ------------------------------------------------------------------
#  Request Full Access to Author: Faizan Saleem Siddiqui
#  Email: faizansaleemsdq@yahoo.com
#  (127 lines of this file are hidden by the author)
# ------------------------------------------------------------------
#
    def matrix(self) -> np.ndarray:
        return quat_to_matrix(self.q)
