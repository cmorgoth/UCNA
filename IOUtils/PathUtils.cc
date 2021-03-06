#include "PathUtils.hh"
#include "strutils.hh"
#include <cassert>
#include <dirent.h>
#include <algorithm>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>


bool fileExists(std::string f) {
	std::string s = "test -r '";
	s += f;
	s += "'";
	return !system(s.c_str());
}

bool dirExists(std::string d) {
	std::string s = "test -d '";
	s += d;
	s += "'";
	return !system(s.c_str());
}

void makePath(std::string p, bool forFile) {
	std::vector<std::string> pathels = split(p,"/");
	if(forFile && pathels.size())
		pathels.pop_back();
	if(!pathels.size())
		return;
	std::string thepath;
	if(p[0]=='/')
		thepath += "/";
	for(unsigned int i=0; i<pathels.size(); i++) {
		thepath += pathels[i] + "/";
		if(!dirExists(thepath)) {
			std::string cmd = std::string("mkdir -p '")+thepath+"'";
			int err = system(cmd.c_str());
			if(err) printf("Error %i creating path '%s'\n",err,thepath.c_str());
			assert(dirExists(thepath));
		}
	}
}

double fileAge(const std::string& fname) {
	if(!(fileExists(fname) || dirExists(fname)))
		return -1.;
	struct stat attrib;
	stat(fname.c_str(), &attrib);
	time_t timenow = time(NULL);
	return timenow - attrib.st_mtime;
}

std::vector<std::string> listdir(const std::string& dir, bool includeHidden) {
	std::vector<std::string> dirs;
	dirent* entry;
	DIR* dp = opendir(dir.c_str());
	if (dp == NULL)
		return dirs;
	while((entry = readdir(dp)))
		if(includeHidden || entry->d_name[0] != '.')
			dirs.push_back(entry->d_name);
	closedir(dp);
	std::sort(dirs.begin(),dirs.end());
	return dirs;
}

std::string getEnvSafe(const std::string& v, const std::string& dflt) {
	const char* envv = getenv(v.c_str());
	if(!envv) {
		if(dflt == "FAIL_IF_MISSING") {
			printf("*** FAIL *** Request environment variable '%s' missing!\n",v.c_str());
			assert(false);
		}
		return dflt;
	}
	return envv;
}
