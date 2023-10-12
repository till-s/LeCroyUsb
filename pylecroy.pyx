# cython: c_string_type=str, c_string_encoding=ascii
# ^^^^^^ DO NOT REMOVE THE ABOVE LINE ^^^^^^^

from cpython.exc cimport *
from cpython     cimport *

cdef extern from "LeCroy.h":
  cdef cppclass LeCroy:
    LeCroy() except+
    int snd_str( const char * ) except+
    int rcv_str( const char *, size_t ) except+

cdef class LeCroyUsb:
  cdef LeCroy c_cls

  def __init__(self):
    pass

  def snd(self, str val):
    put = self.c_cls.snd_str( val )
    self.c_cls.snd_str('\r\n')
    return put

  def rcvto(self, pyb):
    cdef Py_buffer b
    if ( not PyObject_CheckBuffer( pyb ) or 0 != PyObject_GetBuffer( pyb, &b, PyBUF_C_CONTIGUOUS | PyBUF_WRITEABLE ) ):
      raise ValueError("rcvto.read arg must support buffer protocol")
    if ( b.itemsize != 1 ):
      PyBuffer_Release( &b )
      raise ValueError("rcvto.read arg must have element size 1")
    rv = self.c_cls.rcv_str( <char*>b.buf, b.len )
    PyBuffer_Release( &b )
    return rv

  def rcv(self):
    b = bytearray(2048)
    got = self.rcvto( b )
    return b[0:got].decode('ascii')

