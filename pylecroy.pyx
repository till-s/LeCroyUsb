# cython: c_string_type=str, c_string_encoding=ascii
# ^^^^^^ DO NOT REMOVE THE ABOVE LINE ^^^^^^^

from cpython.exc cimport *
from cpython     cimport *

cdef extern from "LeCroy.h":
  cdef cppclass LeCroy:
    LeCroy() except +
    int snd_str( const char * ) except +
    int rcv_str( const char *, size_t ) except +

cdef class LeCroyUsb:
  cdef LeCroy *c_cls
  # Note if we use a non-pointer variable
  # as in
  #   cdef LeCroy c_cls
  # then the object is constructed but exceptions
  # are *not* handled!
  # Work around by using a pointer...

  def __cinit__(self):
    self.c_cls = new LeCroy();

  def __dealloc__(self):
    del self.c_cls

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

  def rcv(self,int sz=2048):
    b = bytearray(2048)
    got = self.rcvto( b )
    return b[0:got].decode('ascii')

  def chat(self, str cmd, int sz=2048):
    self.snd( cmd )
    if cmd[-1] == "?":
      return self.rcv(sz)
