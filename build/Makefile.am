lib_LIBRARIES = libjobsignaler.a
libjobsignaler_a_SOURCES = src/jobsignaler.h src/jobsignaler.c
libjobsignaler_a_CXXFLAGS = -O2
libjobsignaler_a_LDFLAGS = -lpthread -lrt -lm

bin_PROGRAMS = application
applicationdir = src/test
application_SOURCES = src/test/application.c
application_HEADERS = src/jobsignaler.h
application_LDADD = libjobsignaler.a
application_LDFLAGS = -lpthread -lrt -lm
