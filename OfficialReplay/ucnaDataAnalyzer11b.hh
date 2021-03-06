#ifndef UCNADATAANALYZER11B
#define UCNADATAANALYZER11B 1

#include "OutputManager.hh"
#include "TChainScanner.hh"
#include "Enums.hh"
#include "Types.hh"
#include "EnergyCalibrator.hh"
#include "CalDBSQL.hh"
#include "WirechamberReconstruction.hh"
#include "ManualInfo.hh"
#include "RollingWindow.hh"

const size_t kMWPCWires = 16;	//< maximum number of MWPC wires (may be less if some dead)
const size_t kNumModules = 5;	//< number of DAQ modules for internal event header checks
const size_t kNumUCNMons = 4;	//< number of UCN monitors

enum UCN_MON_ID {
	UCN_MON_GV = 0,
	UCN_MON_SW = 1,
	UCN_MON_FE = 2,
	UCN_MON_SCS = 3
};

/// simple class for cuts from Stringmap
class RangeCut {
public:
	/// constructor
	RangeCut(const Stringmap& m = Stringmap());
	/// check if value is in range
	inline bool inRange(double x) const { return start <= x && x <= end; }
	
	double start;	//< cut minimum
	double end;		//< cut maximum
};

/// simple class for value + cuts range
class CutVariable {
public:
	/// constructor
	CutVariable(std::string sn=""): sname(sn) {}
	/// check if in range
	inline bool inRange() const { return R.inRange(val); }
	std::string sname;	//< sensor name (for pedestal subtraction)
	Float_t val;		//< stored value
	RangeCut R;			//< cuts range
};

/// cut blip in data
struct Blip {
	Blip(): start(0), end(0) {}
	BlindTime start;
	BlindTime end;
	BlindTime length() const { return end-start; }
};

/// new clean-ish re-write of data analyzer; try to be backward compatible with old output tree
class ucnaDataAnalyzer11b: public TChainScanner, public OutputManager {
public:
	/// constructor
	ucnaDataAnalyzer11b(RunNum R, std::string bp, CalDB* CDB);
	/// destructor
	virtual ~ucnaDataAnalyzer11b() {}
	
	/// run analysis
	void analyze();
	/// set output DB connection
	inline void setOutputDB(CalDBSQL* CDB = NULL) { CDBout = CDB; }
	/// set to ignore beam cuts
	inline void setIgnoreBeamOut(bool ibo) { ignore_beam_out = ibo; }
	
	/// beam + data cuts
	bool passesBeamCuts();
	/// overall wirechamber cut
	inline bool passedMWPC(Side s) const { return fPassedCathMax[s]; } 
	/// scintillator TDC cut
	inline bool passedCutTDC(Side s) const { return fScint_tdc[s][nBetaTubes].inRange(); }
	/// overall wirechamber+scintillator cut
	inline bool Is2fold(Side s) const { return passedMWPC(s) && passedCutTDC(s); }
	/// overall muon tag check
	inline bool taggedMuon() const { return fTaggedBack[EAST] || fTaggedBack[WEST] || fTaggedDrift[EAST] || fTaggedDrift[WEST] || fTaggedTop[EAST]; }
	/// sis-tagged LED events
	inline bool isLED() const {return int(fSis00) & (1<<7); }
	/// sis-tagged UCN Mon events
	inline bool isUCNMon() const { return int(fSis00) & ((1<<2) | (1<<8) | (1<<9) | (1<<10) | (1<<11)); }
	/// specific UCN monitors
	inline bool isUCNMon(unsigned int n) const { return (int(fSis00) & (1<<2)) && (int(fSis00) & (1<<(8+n))); }
	/// sis-tagged scintillator trigger events
	inline bool isScintTrigger() const { return int(fSis00) & 3; }
	/// figure out whether this is a Bi pulser trigger
	bool isPulserTrigger();
	/// whether one PMT fired
	inline bool pmtFired(Side s, unsigned int t) const { return fScint_tdc[s][t].val > 5; }
	/// whether 2-of-4 trigger fired
	inline bool trig2of4(Side s) const { return fScint_tdc[s][nBetaTubes].val > 5; }
	/// calculate number of PMTs firing on given side
	unsigned int nFiring(Side s) const;
	/// qadc sum
	inline double qadcSum(Side s) const { double q = 0; for(unsigned int t=0; t<nBetaTubes; t++) q+=sevt[s].adc[t]; return q; }
	
protected:
	// whole run variables
	RunNum rn;									//< run number for file being processed
	PMTCalibrator PCal;							//< PMT Calibrator for this run
	CalDBSQL* CDBout;							//< output database connection
	std::vector<Float_t> kWirePositions[2][2];	//< wire positions on each [side][xplane]
	std::vector<std::string> cathNames[2][2];	//< cathode sensor names on each [side][xplane]
	Float_t fAbsTime;							//< absolute time during run
	Float_t fAbsTimeStart;						//< absolute start time of run
	Float_t fAbsTimeEnd;						//< absolute end time of run
	BlindTime deltaT;							//< time scaler wraparound fix
	BlindTime totalTime;						//< total running time (accumulated from last event); after scanning, total ``live'' time (less global cuts)
	Float_t wallTime;							//< initial estimate of run time before actually scanning events; after scanning, total run time
	bool ignore_beam_out;						//< whether to ignore long beam outages (e.g. for a source run)
	Float_t nFailedEvnb;						//< total Evnb failures
	Float_t nFailedBkhf;						//< total Bkhf failures
	RangeCut ScintSelftrig[2];					//< self-trigger range cut for each scintillator (used for Type I origin side determination)
	static ManualInfo MI;								//< source for manual cuts info
	std::vector< std::pair<double,double> > manualCuts;	//< manually cut time segments
	std::vector<Blip> cutBlips;							//< keep track of cut run time
	
	
	// event variables read in, re-calibrated as necessary
	Float_t fTriggerNumber;					//< event trigger number
	Float_t fSis00;							//< Sis00 trigger flags
	BlindTime fTimeScaler;					//< absolute event time scaler, blinded E, W, and unblinded
	CutVariable fBeamclock;					//< time since last beam pulse scaler
	CutVariable fScint_tdc[2][nBetaTubes+1];//< TDC readout for each PMT and side
	ScintEvent sevt[2];						//< scintillator event, for reconstructing energy
	CutVariable fMWPC_anode[2];				//< anode ADC
	Float_t fMWPC_caths[2][2][kMWPCWires];	//< cathodes on [side][xplane][wire]
	Float_t fDelt0;							//< time since previous event
	Float_t fEvnb[kNumModules];				//< header and footer counters per module
	Float_t fBkhf[kNumModules];				//< header and footer counters per module
	CutVariable fBacking_tdc[2];			//< muon backing veto TDC
	Float_t fBacking_adc[2];				//< muon backing veto ADC
	CutVariable fDrift_tac[2];				//< muon veto drift tubes TAC
	CutVariable fTop_tdc[2];				//< top veto TDCs (only East)
	Float_t fTop_adc[2];					//< top veto ADCs (only East)
	CutVariable fMonADC[kNumUCNMons];		//< UCN monitor ADCs = GV, Sw, Fe, SCS
	RollingWindow gvMonChecker;				//< rolling window check on gv monitor rate
	bool prevPassedCuts;					//< whether passed cuts on previous event
	bool prevPassedGVRate;					//< whether passed GV rate on previous event
	
	/// load a cut range for a CutVariable
	void loadCut(CutVariable& c, const std::string& cutName);
	/// load all cuts for run
	void loadCuts();
	
	/// set read points for input TChain
	virtual void setReadpoints();
	/// setup output tree, read points
	void setupOutputTree();
	
	// additional event variables for output tree
	TTree* TPhys;			//< output tree
	Int_t fEvnbGood;		//< DAQ data quality checks
	Int_t fBkhfGood;		//< DAQ data quality checks
	Int_t fPassedAnode[2];	//< whether passed anode cut on each side 
	Int_t fPassedCath[2];	//< whether passed cathode sum cut on each side
	Int_t fPassedCathMax[2];//< whether passed cathode max cut on each side
	Int_t fPassedGlobal;	//< whether this event passed global beam/misc cuts on each side (counts for total run time)
	wireHit wirePos[2][2];	//< wire positioning data for [side][direction]
	CutVariable fCathSum[2];//< combined x+y cathode sum on each side
	CutVariable fCathMax[2];//< min(max cathode each plane) for each side
	Float_t fEMWPC[2];		//< reconstructed energy deposition in wirechamber
	UInt_t fTaggedBack[2];	//< whether event was tagged by the muon backing veto on each side
	UInt_t fTaggedDrift[2];	//< whether event was tagged by muon veto drift tubes on each side
	UInt_t fTaggedTop[2];	//< whether event was tagged by top veto on each side (only meaningful on East)
	Side fSide;				//< event primary scintillator side
	EventType fType;		//< event backscatter type
	PID fPID;			//< event particle ID
	Float_t fEtrue;			//< event reconstructed true energy
	
	/// pre-scan data to extract pedestals
	void pedestalPrePass();
	/// fit pedestals
	void monitorPedestal(std::vector< std::pair<float,float> > dpts, const std::string& mon_name, double graphWidth);
	
	/*--- event processing loop ---*/
	/// process current event raw->phys
	void processEvent();
	/// check event headers for errors
	void checkHeaderQuality();
	/// fix scaler overflows, convert times to seconds
	void calibrateTimes();
	/// reconstruct wirechamber positions
	void reconstructPosition();
	/// apply PMT calibrations to get visible energy
	void reconstructVisibleEnergy();
	/// classify muon veto response
	void checkMuonVetos();
	/// classify event type
	void classifyEventType();
	/// reconstruct true energy based on event type
	void reconstructTrueEnergy();
	
	/*--- end of processing ---*/
	/// trigger efficiency curves
	void calcTrigEffic();
	/// Bi pulser gain stabilizer
	void processBiPulser();
	/// tally total run time
	void tallyRunTime();
	/// output replay summary info, optionally into AnalysisDB
	void replaySummary();
	/// print info to replace old "quick analyzer"
	void quickAnalyzerSummary() const;
	
	
	/*--- histograms ---*/
	/// set up summary histograms
	void setupHistograms();
	/// fill summary histograms with few calibration dependencies
	void fillEarlyHistograms();
	/// fill summary histograms from event data
	void fillHistograms();
	/// locate sources on positions histogram
	void locateSourcePositions();
	/// output histograms of interest
	void plotHistos();
	/// draw cut range lines
	void drawCutRange(const RangeCut& r, Int_t c=4);
	/// draw regions excluded by blip cuts
	void drawExclusionBlips(Int_t c=4);
	// histograms
	TH1F* hCathMax[2][2];		//< cathode max, [side][cut]
	TH1F* hAnode[2][2];			//< anode, [side][cut]
	TH1F* hCathSum[2][2];		//< cathode sum, [side][cut]
	TH1F* hBackTDC[2];			//< backing TDC
	TH1F* hBackADC[2][2];		//< backing ADC, [side][cut]
	TH1F* hDriftTAC[2];			//< drift TAC [side]
	TH1F* hTopTDC[2];			//< top TDC [side] (East only)
	TH1F* hTopADC[2][2];		//< top ADC [side][cut] (East only)
	TH1F* hScintTDC[2];			//< scintillator 2-of-4 TDC by [side]
	TH1F* hEtrue[2][5];			//< true energy, by [side][type]
	TH1F* hTuben[2][nBetaTubes];//< individual PMT visible energy
	TH1F* hMonADC[kNumUCNMons];	//< UCN Monitor ADCs
	TH1F* hMonRate[kNumUCNMons];//< UCM Monitor rates
	TH1F* hTypeRate[3];			//< rate of event type for betas
	TH1F* hSideRate[2][2];		//< rate for [side][muon/beta]
	TH1F* hBkhfFailRate;		//< rate of bad Bkhf events
	TH1F* hEvnbFailRate;		//< rate of bad Evnb events
	TH1F* hHitsProfile[2][2];	//< 1D hit position histograms
	TH2F* hHitPos[2];			//< hit position on each side, 2D
	TH1F* hTrigEffic[2][nBetaTubes][2];			//< trigger efficiency for [side][tube][all/trig]
	std::vector<TH1*> hBiPulser[2][nBetaTubes];	//< Bi puser for [side][tube]
};


#endif
