CXX = g++

OPTIONS  = -O3 -std=c++1y
WARNINGS = -Wall -Wextra -Werror

MAIN 	= src/main.o
OBJECTS = src/xml2json.gch
EXEC 	= xml2json

#############################################################

all : ${EXEC}

src/xml2json.gch : src/xml2json.hpp
	${CXX} ${OPTIONS} -c $< -o $@

${MAIN} : src/main.cpp
	${CXX} ${OPTIONS} -c $< -o $@

${EXEC} : ${MAIN} ${OBJECTS}
	${CXX} ${OPTIONS} ${MAIN} -o ${EXEC}

clean :
	rm src/*.gch src/*.o ${EXEC}
