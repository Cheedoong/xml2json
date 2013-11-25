#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <fstream>
#include <sys/time.h>
#include <time.h>

#include "xml2json.hpp"

using namespace std;

int64_t getCurrentTime()
{
	struct timeval tval;

	gettimeofday(&tval, NULL);
	return tval.tv_sec * 1000000LL + tval.tv_usec;
}

int main(int argc, const char *argv[])
{
	//file<> fdoc("track_orig.xml");
	string xml_str, json_str;
	ifstream inf;

	inf.open("test_track_1.xml");
	ostringstream oss;
	oss << inf.rdbuf();
	xml_str = oss.str();
	inf.close();
	int64_t start_time = getCurrentTime();
	json_str = xml2json(const_cast<char *>(xml_str.c_str()));
	int64_t end_time = getCurrentTime();
	cerr << "time: " << end_time - start_time << endl;

	ofstream outf;
	outf.open("test_track_1.js.txt");
	outf << json_str;
	outf.close();
	return 0;
}
