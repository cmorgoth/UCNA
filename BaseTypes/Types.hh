/// \file Types.hh \brief basic utility classes and functions
#ifndef TYPES_HH
/// make sure this file is included only once
#define TYPES_HH 1

#include "Enums.hh"
#include "QFile.hh"
#include "strutils.hh"

#include <TBranch.h>
#include <string>
#include <TH1.h>
#include <TH1F.h>
#include <TF1.h>
#include <TFile.h>
#include <TMath.h>
#include <map>
#include <cassert>

/// CDF for poisson function (for trigger efficiency fits)
Double_t poiscdf(const Double_t *x, const Double_t *par);
/// Fancier trigger efficiency model
Double_t fancyfish(const Double_t *x, const Double_t *par);

/// Whether a point falls inside an ellipse
inline bool pointIsInEllipse(float x, float y, float wx, float wy) { return x*x/(wx*wx) + y*y/(wy*wy) < 1.0; }
/// interpolate fractional bin positions from a TAxis (needed to interpret TSpectrum positions)
double binterpolate(const TAxis* ax, double binloc);

/// zero out a histogram
void zero(TH1* h);

/// get total number of bins in a (multi-dimensional) histogram
unsigned int totalBins(const TH1* h);

/// blinded times
class BlindTime {
public:
	/// constructor from time
	BlindTime(Float_t t0=0) { t[EAST]=t[WEST]=t[BOTH]=t[NONE]=t0; }
	/// constructor from Stringmap
	BlindTime(const Stringmap& m);
	/// convert to Stringmap
	Stringmap toStringmap() const;
	/// add another blinded time
	inline void operator+= (const BlindTime& bt) { for(Side s=EAST; s!=NONE; ++s) t[s] += bt.t[s]; }
	/// subtract another blinded time
	inline void operator-= (const BlindTime& bt) { for(Side s=EAST; s!=NONE; ++s) t[s] -= bt.t[s]; }
	
	Float_t t[4];	//< blinded timings for each side
};
/// add blinded times
BlindTime operator+(const BlindTime& a, const BlindTime& b);
/// subtract blinded times
BlindTime operator-(const BlindTime& a, const BlindTime& b);
/// scale blinded times
BlindTime operator*(double x, const BlindTime& a);

/// stored data for triggers/scalers
struct TrigInfo {
	float runClock;		//< time since begin of run (s)
	float beamClock;	//< time since beam pulse (s)
	RunNum runNum;		//< run number for this event (useful when output trees TChain'd together)
	UInt_t e;			//< event number in raw data
	Int_t trigflags;	//< trigger flags
	float dt;			//< time since the previous event
};

/// stored data for a Beta Scintillator event
struct ScintEvent {
	Float_t adc[nBetaTubes];		//< raw pedestal-subtracted PMT ADCs
	float_err tuben[nBetaTubes];	//< individual tube reconstructed energies
	float_err energy;				//< total reconstructed energy
	Float_t nPE[nBetaTubes];		//< number of PE seen by each PMT
};

/// stored data for an MWPC event
struct MWPCevent {
	Float_t cathodeSum;	//< sum of constituent wirechamber cathodes
	Float_t anode;		//< anode ADC
	Int_t sourceID;		//< identified source region in wirechamber
	Float_t pos[2];		//< x/y position
	Int_t errflags[2];	//< reconstruction error flags
};

/// relevant flags for a beta hit event
struct BetaHit {
	Int_t typeFlags[2];	//< event type flags
	Int_t sourceID;		//< originating source ID for source events
};

#endif


