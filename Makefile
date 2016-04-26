#CXX = 

UNAME := $(shell uname -s)
ifeq ($(UNAME), Darwin)
LINKOPTIONS = -Wl,-search_paths_first -Wl,-dead_strip -v
else
LINKOPTIONS = -Wl,--gc-sections -Wl,--strip-all
endif

INCLUDE += -I./include/
COMPILEOPTIONS  = -std=c++11 -O3 -fdata-sections -ffunction-sections
WARNINGS = -Wall -Wextra -Werror

MAIN 	= xml2json.o
OBJECTS = xml2json.gch
EXEC 	= xml2json

#############################################################

all : ${EXEC}

xml2json.gch : include/xml2json.hpp
	${CXX} ${COMPILEOPTIONS}  -c $< -o $@

${MAIN} : xml2json.cpp
	${CXX} ${COMPILEOPTIONS} $(INCLUDE) -c $< -o $@

${EXEC} : ${MAIN} ${OBJECTS}
	${CXX} ${LINKOPTIONS} ${MAIN} -o ${EXEC}

clean :
	rm *.gch *.o ${EXEC}
