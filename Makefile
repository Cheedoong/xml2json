CXX = g++

INCLUDE += -I./include/
OPTIONS  = -std=c++11 -O3
WARNINGS = -Wall -Wextra -Werror

MAIN 	= xml2json.o
OBJECTS = xml2json.gch
EXEC 	= xml2json

#############################################################

all : ${EXEC}

xml2json.gch : include/xml2json.hpp
	${CXX} ${OPTIONS}  -c $< -o $@

${MAIN} : xml2json.cpp
	${CXX} ${OPTIONS} $(INCLUDE) -c $< -o $@

${EXEC} : ${MAIN} ${OBJECTS}
	${CXX} ${OPTIONS} ${MAIN} -o ${EXEC}

clean :
	rm *.gch *.o ${EXEC}
