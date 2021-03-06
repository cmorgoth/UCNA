#include "Enums.hh"
#include <math.h>
#include <stdio.h>
#include <cassert>

char sideNames(Side s) {
	const char snm[] = {'E','W','B','N'};
	return snm[s];
};

const char* sideWords(Side s) {
	assert(s<=BADSIDE);
	const char* swd[] = {"East","West","Both","None","BadSide"};
	return swd[s];
}

std::string typeWords(EventType tp) {
	assert(tp<=TYPE_IV_EVENT);
	const char* twd[] = {"Type0","TypeI","TypeII","TypeIII","TypeIV"};
	return twd[tp];
}


// TODO: make this robust
std::string sideSubst(std::string instr, Side s) {
	char tmp[1024];
	if(instr.find("%c") != instr.npos)
		sprintf(tmp,instr.c_str(),sideNames(s));
	if(instr.find("%s") != instr.npos)
		sprintf(tmp,instr.c_str(),sideWords(s));
	return std::string(tmp);
}

Side otherSide(Side s) {
	switch(s) {
		case EAST:
			return WEST;
		case WEST:
			return EAST;
		case BOTH:
			return NONE;
		default:
			return BOTH;
	}
}

unsigned int pmtHardwareNum(Side s, unsigned int quadrant) {
	if(s==EAST)
		return (quadrant+2)%4+1;
	if(s==WEST)
		return quadrant+1;
	return 0;
}

float_err operator+(float_err a, float_err b) { return float_err(a.x+b.x,sqrt(a.err*a.err + b.err*b.err)); }

float_err operator*(float a, float_err b) { return float_err(a*b.x,a*b.err); }

float_err weightedSum(unsigned int n, const float_err* d) {
	float_err sum;
	sum.x = sum.err = 0;
	for(unsigned int i=0; i<n; i++) {
		sum.x += d[i].x/(d[i].err*d[i].err);
		sum.err += 1/(d[i].err*d[i].err);
	}
	sum.x /= sum.err;
	sum.err = 1/sqrt(sum.err);
	return sum;
}

float proximity(unsigned int n, const float_err* d, float_err c) {
	float psum = 0;
	for(unsigned int i=0; i<n; i++)
		psum += (d[i].x-c.x)*(d[i].x-c.x)/(d[i].err*d[i].err + c.err*c.err);
	return sqrt(psum/n);
}

RunGeometry whichGeometry(RunNum rn) {
	if( rn < 1000 )
		return GEOMETRY_OTHER;
	if( rn < 7000 )
		return GEOMETRY_2007;
	if( rn < 9220)
		return GEOMETRY_A;
	if( rn < 10400)
		return GEOMETRY_B;
	if( rn < 11390)
		return GEOMETRY_C;
	if( rn < 11501)
		return GEOMETRY_D;
	return GEOMETRY_C;
}

std::string geomName(RunGeometry g) {
	if( g == GEOMETRY_2007 )
		return "2007";
	if( g == GEOMETRY_A )
		return "A";
	if( g == GEOMETRY_B )
		return "B";
	if( g == GEOMETRY_C )
		return "C";
	if( g == GEOMETRY_D )
		return "D";
	return "OTHER";
}


