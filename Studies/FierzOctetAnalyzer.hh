#ifndef OCTETANALYZEREXAMPLE_HH
#define OCTETANALYZEREXAMPLE_HH 1

#include "OctetAnalyzer.hh"
#include "RunAccumulator.hh"
#include "PathUtils.hh"

using namespace std;

/// minimalist example subclass of OctetAnalyzer: generates super-ratio and super-sum of wirechamber energy spectra
class FierzOctetAnalyzer: public OctetAnalyzer {
public:
	/// constructor
	FierzOctetAnalyzer(OutputManager* pnt, const string& nm, const string& inflname = "");
	/// destructor... you probably won't need to destruct anything
	~FierzOctetAnalyzer() {}
	
	/// cloning generator: just return another of the same subclass (with any settings you want to preserve)
	virtual SegmentSaver* makeAnalyzer(const string& nm,
									   const string& inflname) { 
		return new FierzOctetAnalyzer(this,nm,inflname); 
	}
	
	/// location of already-processed data (after first run) for errorbar estimation
	virtual string estimatorHistoLocation() const { return FierzOctetAnalyzer::processedLocation; }
	static string processedLocation;	//< set location here for already-processed files
	
	
	/// fill from scan data point
	virtual void fillCoreHists(ProcessedDataScanner& PDS, double weight);
	/// calculate super-ratio asymmetry from anode spectra
	virtual void calculateResults();
	/// output plot generation
	virtual void makePlots();
	/// MC/Data comparison routine
	virtual void compareMCtoData(RunAccumulator& OAdata, float simfactor);
	
	/*
	quadHists qAnodeSpectrum[2];	//< set of histograms for extracting anode spectrum on each side
	TH1F* hAnodeSpectrum[2];		//< convenient pointer for currently active histogram
	TH1F* hAnodeSR;					//< super-ratio asymmetry of anode data
	TH1F* hAnodeSS;					//< super-sum of anode data
*/

	quadHists qFullEnergySpectrum[2];		//< set of histograms for extracting anode spectrum on each side
	TH1F* hFullEnergySpectrum[2];			//< convenient pointer for currently active histogram
	TH1F* hFullEnergySR;					//< super-ratio asymmetry of anode data
	TH1F* hFullEnergySS;					//< super-sum of anode data
};

#endif
