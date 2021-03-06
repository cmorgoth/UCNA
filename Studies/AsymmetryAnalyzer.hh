#ifndef ASYMMETRYANALYZER_HH
#define ASYMMETRYANALYZER_HH 1

#include "OctetAnalyzer.hh"

/// primary octet data analysis class
class AsymmetryAnalyzer: public OctetAnalyzer {
public:
	/// constructor
	AsymmetryAnalyzer(OutputManager* pnt, const std::string& nm, const std::string& inflname = "");
	
	/// cloning generator
	virtual SegmentSaver* makeAnalyzer(const std::string& nm,
									   const std::string& inflname) { return new AsymmetryAnalyzer(this,nm,inflname); }
	
	/// MC/data comparison
	void compareMCtoData(RunAccumulator& OAdata, float simfactor);
	
	/// location of already-processed data (after first run) for errorbar estimation
	virtual std::string estimatorHistoLocation() const { return AsymmetryAnalyzer::processedLocation; }
	static std::string processedLocation;	//< set location here for already-processed files
	
protected:
	
	/// fill from scan data point
	virtual void fillCoreHists(ProcessedDataScanner& PDS, double weight);
	/// calculate super-ratio asymmetry from anode spectra
	virtual void calculateResults();
	/// output plot generation
	virtual void makePlots();
	
	/// fit asymmetry over given range
	void fitAsym(float fmin, float fmax, unsigned int color);
	/// various beta spectrum endpoint fits
	void endpointFits();
	/// anode calibration fits
	void anodeCalFits();
	
	static TF1 asymmetryFit;	//< fit function for asymmetry
	static AnalysisChoice anChoice;	//< asymmetry analysis choice
	
	quadHists qEnergySpectra[2][TYPE_IV_EVENT+1];	//< energy spectra quad hists for [side][event type]
	TH1F* hEnergySpectra[2][TYPE_IV_EVENT+1];		//< energy spectra write point for [side][event type]
	quadHists qPositions[2][TYPE_III_EVENT+1];		//< event positions quad hists for [side][type]
	TH2F* hPositions[2][TYPE_III_EVENT+1];			//< event positions write point for [side][type]
	quadHists qAnodeCal[2];							//< anode calibration spectrum (Type 0, Erecon>225)
	TH1F* hAnodeCal[2];								//< anode cal write point
	
	TH1F* hAsym;									//< asymmetry
	TH1F* hSuperSum;								//< super-sum spectrum
};

#endif
