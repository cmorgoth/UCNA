#ifndef SEGMENTSAVER_HH
#define SEGMENTSAVER_HH 1

#include "OutputManager.hh"
#include <TH1.h>
#include <TFile.h>
#include <map>
#include <string>

/// class for saving, retrieving, and summing histograms from file
class SegmentSaver: public OutputManager {
public:
	/// constructor, optionally with input filename
	SegmentSaver(OutputManager* pnt, const std::string& nm = "SegmentSaver", const std::string& inflName = "");
	/// destructor
	virtual ~SegmentSaver();
	/// get location of this analyzer's input file
	const std::string& getInflName() const { return inflname; }
	
	/// generate or restore from file a saved TH1F histogram
	TH1* registerSavedHist(const std::string& hname, const std::string& title,unsigned int nbins, float xmin, float xmax);
	/// generate or restore from file a saved histogram from a template
	TH1* registerSavedHist(const std::string& hname, const TH1& hTemplate);
	
	/// get core histogram by name
	TH1* getSavedHist(const std::string& hname);
	/// get core histogram by name, const version
	const TH1* getSavedHist(const std::string& hname) const;
	/// zero out all saved histograms
	virtual void zeroSavedHists();
	
	/// add histograms from another SegmentSaver of the same type
	virtual void addSegment(const SegmentSaver& S);
	/// check if this is equivalent layout to another SegmentSaver
	virtual bool isEquivalent(const SegmentSaver& S) const;
	
	// ----- Subclass me! ----- //
	
	/// create a new instance of this object (cloning self settings) for given directory
	virtual SegmentSaver* makeAnalyzer(const std::string& nm, const std::string& inflname) = 0;
	/// virtual routine for generating output plots
	virtual void makePlots() = 0;
	/// virtual routine for generating calculated hists
	virtual void calculateResults() = 0;
	
protected:
	
	std::map<std::string,TH1*> saveHists;		//< saved histograms
	TFile* fIn;									//< input file to read in histograms from
	std::string inflname;						//< where to look for input file
};

#endif
