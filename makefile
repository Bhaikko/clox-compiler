CXX = g++
CPPFLAGS = -std=c++11 -Wall -g

LIBS_CPPS = \
				./lib/memory.c \
				./lib/chunk.c \

SRCS_CPPS = \
				./src/main.cpp \

run:
	$(CXX) $(LIBS_CPPS) $(SRCS_CPPS) -o application $(CPPFLAGS)