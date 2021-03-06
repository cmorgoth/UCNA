#ifndef ENDPOINTSTUDY_HH
#define ENDPOINTSTUDY_HH 1

#include "GraphicsUtils.hh"
#include "SectorCutter.hh"
#include "EnergyCalibrator.hh"
#include "PMTGenerator.hh"
#include "DataSource.hh"
#include "KurieFitter.hh"
#include "RunAccumulator.hh"

/// data collected for each sector
struct SectorDat {
	Side s;
	unsigned int t;
	unsigned int m;
	float eta;
	float_err low_peak;
	float_err low_peak_width;
	float_err xe_ep;
};

/// class for position-segmented analysis
class PositionBinner: public RunAccumulator {
public:
	/// constructor
	PositionBinner(OutputManager* pnt, const std::string& nm, float r, unsigned int nr, const std::string& infl="");
	
	/// cloning generator: just return another of the same subclass (with any settings you want to preserve)
	virtual SegmentSaver* makeAnalyzer(const std::string& nm,
									   const std::string& inflname) { return new PositionBinner(this,nm,sects.r,sects.n,inflname); }
	
	/// process a data point into position histograms
	virtual void fillCoreHists(ProcessedDataScanner& PDS, double weight);
	
	/// determine what section a point belongs to
	virtual unsigned int getSector(float x, float y) { return sects.sector(x,y); }
	/// get number of sectors
	virtual unsigned int getNSectors() const { return sects.nSectors(); }
	
	/// fit enpoints in each sector
	virtual void calculateResults();
	/// make output plots
	virtual void makePlots();
	/// virtual routine for MC/Data comparison plots/calculations
	virtual void compareMCtoData(RunAccumulator& OAdata, float simfactor);
		
protected:
	std::vector<fgbgPair> sectEnergy[2][nBetaTubes];	//< visible light histograms by position, PMT
	std::vector<SectorDat> sectDat[2][nBetaTubes];		//< processed data for each sector
	fgbgPair energySpectrum[2];							//< combined energy spectrum, each side
	std::vector<fgbgPair> sectAnode[2];					//< anode histograms by position
	SectorCutter sects;									//< sector cutter
};

/// process xenon runs
void process_xenon(RunNum r0, RunNum r1, unsigned int nrings);

/// compare xenon to simulation
void simulate_xenon(RunNum r0, RunNum r1);
 
#endif
