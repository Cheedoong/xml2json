#C=gcc
#CXX = g++

UNAME := $(shell uname -s)
ifeq ($(UNAME), Darwin)
LINKOPTIONS = -Wl,-search_paths_first -Wl,-dead_strip -v
else
LINKOPTIONS = -Wl,--gc-sections -Wl,--strip-all
endif

COMPILEOPTIONS  = -std=c++11 -O3 -fdata-sections -ffunction-sections
INCLUDE += -I../include/
LIB +=

TARGET=test
ALL:$(TARGET)

C_SRCFILES := $(wildcard *.c)
CPP_SRCFILES := $(wildcard *.cpp)

C_OBJS := $(patsubst %.c,%.o, $(C_SRCFILES))
CPP_OBJS := $(patsubst %.cpp,%.o, $(CPP_SRCFILES))

OBJS := $(C_OBJS) $(CPP_OBJS)

$(TARGET):$(OBJS)
	$(CXX) $(LINKOPTIONS) -o $@ $^ $(LIB)

%.o: %.cpp
	$(CXX) $(COMPILEOPTIONS) $(INCLUDE) -c -o $@ $<

%.o: %.c
	$(CC) $(COMPILEOPTIONS) $(INCLUDE) -c -o $@ $<

clean:
	rm -f *.o
	rm -f $(TARGET)
