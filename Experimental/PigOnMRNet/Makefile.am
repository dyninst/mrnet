
INCLUDES = -I@top_srcdir@/../../src/ 
bin_PROGRAMS = boar_frontend boar_backend
AM_CXXFLAGS = -g -pedantic -Wall

boar_backend_SOURCES = boar_backend.C input_reader.C input_reader.h
boar_backend_LDADD = -lmrnet -lxplat -lfl -lm -lpthread -ldl
boar_backend_LDFLAGS = -rdynamic

boar_frontend_SOURCES = boar_frontend.C physical_plan.C physical_plan.h types_visitor.C types_visitor.h pig_types.C pig_types.h
boar_frontend_LDADD = -lmrnet -lxplat -lfl -lm -lpthread -ldl
boar_frontend_LDFLAGS = -rdynamic

lib_LTLIBRARIES = libboarfilter.la
libboarfilter_la_SOURCES = boar_filter.C physical_plan.C physical_plan.h types_visitor.C types_visitor.h pig_types.C pig_types.h
libboarfilter_la_CXXFLAGS = $(AM_CFLAGS)

