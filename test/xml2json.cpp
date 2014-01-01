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
    ofstream outf;
    ostringstream oss;
    char BOM[4]={(char)0xEF, (char)0xBB, (char)0xBF, '\0'};
    int64_t start_time, end_time;

    inf.open("test_track_1.xml");
	outf.open("test_track_1.js.txt");
    oss.clear();
	oss << inf.rdbuf();
	xml_str = oss.str();
	inf.close();
	start_time = getCurrentTime();
	json_str = xml2json(xml_str.c_str());
	end_time = getCurrentTime();
	cout << "time1: " << end_time - start_time << endl;
	outf << BOM << json_str;
	outf.close();

    inf.open("test_track_2.xml");
	outf.open("test_track_2.js.txt");
    oss.clear();
	oss << inf.rdbuf();
	xml_str = oss.str();
	inf.close();
	start_time = getCurrentTime();
	json_str = xml2json(xml_str.c_str());
	end_time = getCurrentTime();
	cout << "time2: " << end_time - start_time << endl;
	outf << BOM << json_str;
	outf.close();

	inf.open("test_track_3.xml");
	outf.open("test_track_3.js.txt");
    oss.clear();
	oss << inf.rdbuf();
	xml_str = oss.str();
	inf.close();
	start_time = getCurrentTime();
	json_str = xml2json(xml_str.c_str());
	end_time = getCurrentTime();
	cout << "time3: " << end_time - start_time << endl;
	outf << BOM << json_str;
	outf.close();

	return 0;
}
