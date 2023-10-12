CFLAGS=-g -Wall -I. -I/usr/include/python3.10 -fPIC

all: lctest pylecroy.so

lctest: lctest.o LeCroy.o
	$(CXX) -o $@ $(CFLAGS) $^ -lusb-1.0

lecroy: lecroy.o
	$(CC) -o $@ $(CFLAGS) $^ -lusb-1.0

lecroy.o: lecroy.c
	$(CC) -c  $(CFLAGS)  $^

%.o: %.cc LeCroy.h
	$(CXX) -c $(CFLAGS)  $<

pylecroy.cc: pylecroy.pyx
	cython3 -3 --cplus -o $@ $^

pylecroy.so: pylecroy.o LeCroy.o
	$(CXX) -shared -o $@ $^ -lusb-1.0

clean:
	$(RM) lecroy.o lecroy LeCroy.o lctest pylecroy.so pylecroy.o lctest.o
