#include "ucnaDataAnalyzer11b.hh"
#include "strutils.hh"
#include "ManualInfo.hh"
#include "GraphicsUtils.hh"
#include "MultiGaus.hh"
#include <stdio.h>
#include <unistd.h>
#include <TStyle.h>
#include <TDatime.h>

RangeCut::RangeCut(const Stringmap& m): start(m.getDefault("start",0.0)), end(m.getDefault("end",0.0)) {}

ManualInfo ucnaDataAnalyzer11b::MI = ManualInfo("../../SummaryData/ManualInfo.txt");

ucnaDataAnalyzer11b::ucnaDataAnalyzer11b(RunNum R, std::string bp, CalDB* CDB):
TChainScanner("h1"), OutputManager(std::string("spec_")+itos(R),bp+"/hists/"), rn(R), PCal(R,CDB), CDBout(NULL),
deltaT(0), totalTime(0), ignore_beam_out(false), nFailedEvnb(0), nFailedBkhf(0), gvMonChecker(5,5.0), prevPassedCuts(true), prevPassedGVRate(true) {
	if(R>16300 && !CDB->isValid(R)) {
		printf("*** Bogus calibration for new runs! ***\n");
		PCal = PMTCalibrator(16000,CDB);
	}
	plotPath = bp+"/figures/run_"+itos(R)+"/";
	dataPath = bp+"/data/";
}

void ucnaDataAnalyzer11b::analyze() {
	loadCuts();
	setupOutputTree();
	
	pedestalPrePass();
	printf("\nRun wall time is %.1fs\n\n",wallTime);
	setupHistograms();
	printf("Scanning input data...\n");
	startScan();
	while (nextPoint())
		processEvent();
	printf("Done.\n");
	processBiPulser();
	calcTrigEffic();
	tallyRunTime();
	locateSourcePositions();
	plotHistos();
	replaySummary();
	
	quickAnalyzerSummary();
}

void ucnaDataAnalyzer11b::loadCut(CutVariable& c, const std::string& cutName) {
	std::vector<Stringmap> v = MI.getInRange(cutName,rn);
	if(v.size() != 1) {
		printf("*** Expected 1 cut but found %i for %s/%i! Fail!\n",int(v.size()),cutName.c_str(),rn);
		assert(false);
	}
	c.R = RangeCut(v[0]);
	printf("Loaded cut %s/%i = (%g,%g)\n",cutName.c_str(),rn,c.R.start,c.R.end);
}

void ucnaDataAnalyzer11b::loadCuts() {
	for(Side s = EAST; s <= WEST; ++s) {
		loadCut(fMWPC_anode[s], sideSubst("Cut_MWPC_%c_Anode",s));
		loadCut(fCathMax[s],sideSubst("Cut_MWPC_%c_CathMax",s));
		loadCut(fCathSum[s],sideSubst("Cut_MWPC_%c_CathSum",s));
		loadCut(fBacking_tdc[s], sideSubst("Cut_TDC_Back_%c",s));
		loadCut(fDrift_tac[s], sideSubst("Cut_ADC_Drift_%c",s));
		loadCut(fScint_tdc[s][nBetaTubes], sideSubst("Cut_TDC_Scint_%c_Selftrig",s));
		ScintSelftrig[s] = fScint_tdc[s][nBetaTubes].R;
		loadCut(fScint_tdc[s][nBetaTubes], sideSubst("Cut_TDC_Scint_%c",s));
	}
	loadCut(fTop_tdc[EAST], "Cut_TDC_Top_E");
	loadCut(fBeamclock,"Cut_BeamBurst");
	if(ignore_beam_out)
		fBeamclock.R.end = FLT_MAX;
	manualCuts = MI.getRanges(itos(rn)+"_timecut");
	if(manualCuts.size())
		printf("Manually cutting %i time ranges...\n",(int)manualCuts.size());
}

void ucnaDataAnalyzer11b::checkHeaderQuality() {
	fEvnbGood = fBkhfGood = true;
	for(size_t i=0; i<kNumModules; i++) {
		if(int(fEvnb[i]-fTriggerNumber)) fEvnbGood = false;
		if(int(fBkhf[i])!=17) fBkhfGood = false;
	}
	nFailedEvnb += !fEvnbGood;
	nFailedBkhf += !fBkhfGood;
}

void ucnaDataAnalyzer11b::calibrateTimes() {
	
	// start/end times
	if(currentEvent==0) {
		fAbsTimeStart = fAbsTime;
		prevPassedCuts = prevPassedGVRate = true;
		totalTime = deltaT = 0;
	}
	fAbsTimeEnd = fAbsTime;
	
	// convert microseconds to seconds
	for(Side s = EAST; s<=BOTH; ++s)
		fTimeScaler.t[s] *= 1.0e-6;	
	fBeamclock.val *= 1.0e-6;	
	fDelt0 *= 1.0e-6;
	
	// check for overflow condition
	if(fTimeScaler.t[BOTH] < totalTime.t[BOTH]-deltaT.t[BOTH]-1000.0) {
		printf("\tFixing timing scaler overflow... ");
		deltaT.t[BOTH] += 4294967296.0*1e-6;
		for(Side s = EAST; s<=WEST; ++s)
			deltaT.t[s] = totalTime.t[s];
	}
	// add overflow wraparound time
	fTimeScaler += deltaT;
	
	if(isUCNMon(UCN_MON_GV))
		gvMonChecker.addCount(fTimeScaler.t[BOTH]);
	else
		gvMonChecker.moveTimeLimit(fTimeScaler.t[BOTH]);
	
	// check global time cuts, add new blips as necessary
	bool passedGVRate = (gvMonChecker.getCount() == gvMonChecker.nMax) || (prevPassedGVRate && gvMonChecker.getCount() > gvMonChecker.nMax/2.0);
	prevPassedGVRate = passedGVRate;
	fPassedGlobal = passesBeamCuts() && (ignore_beam_out || fTimeScaler.t[BOTH]<gvMonChecker.lMax || passedGVRate);
	if(fPassedGlobal != prevPassedCuts) {
		if(!fPassedGlobal) {
			Blip b;
			b.start = 0.5*(fTimeScaler+totalTime);
			cutBlips.push_back(b);
		} else {
			cutBlips.back().end = 0.5*(fTimeScaler+totalTime);
		}
	}
	
	prevPassedCuts = fPassedGlobal;
	totalTime = fTimeScaler;
}

unsigned int ucnaDataAnalyzer11b::nFiring(Side s) const {
	unsigned int nf = 0;
	for(unsigned int t=0; t<nBetaTubes; t++)
		nf += pmtFired(s,t);
	return nf;
}

bool ucnaDataAnalyzer11b::isPulserTrigger() {
	if(int(fSis00) & (1<<5) && !(trig2of4(EAST)||trig2of4(WEST)))
		return true;
	for(Side s = EAST; s <= WEST; ++s) {
		unsigned int nthresh = 0;
		unsigned int nhigh = 0;
		for(unsigned int t=0; t<nBetaTubes; t++) {
			nthresh += (sevt[s].adc[t] > 200);
			nhigh += (sevt[s].adc[t] > 1500);
		}
		if(nhigh == 1 && nthresh == 1)
			return true;
	}
	return false;
}


bool ucnaDataAnalyzer11b::passesBeamCuts() {
	// basic time-since-beam cut
	if(!fBeamclock.inRange())
		return false;	
	// remove manually tagged segments
	for(std::vector< std::pair<double,double> >::const_iterator it = manualCuts.begin(); it != manualCuts.end(); it++)
		if (it->first <= fTimeScaler.t[BOTH] && fTimeScaler.t[BOTH] <= it->second)
			return false;
	return true;
}

void ucnaDataAnalyzer11b::reconstructPosition() {
	for(Side s = EAST; s <= WEST; ++s) {
		for(unsigned int d = X_DIRECTION; d <= Y_DIRECTION; d++) {
			float cathPeds[kMWPCWires];
			for(unsigned int c=0; c<cathNames[s][d].size(); c++)
				cathPeds[c] = PCal.getPedestal(cathNames[s][d][c],fTimeScaler.t[BOTH]);
			wirePos[s][d] = mpmGaussianPositioner(kWirePositions[s][d], fMWPC_caths[s][d], cathPeds);
		}
		fMWPC_anode[s].val -= PCal.getPedestal(sideSubst("MWPC%cAnode",s),fTimeScaler.t[BOTH]);
		fCathSum[s].val = wirePos[s][X_DIRECTION].cathodeSum + wirePos[s][Y_DIRECTION].cathodeSum;
		fCathMax[s].val = wirePos[s][X_DIRECTION].maxValue<wirePos[s][Y_DIRECTION].maxValue?wirePos[s][X_DIRECTION].maxValue:wirePos[s][Y_DIRECTION].maxValue;
		fPassedAnode[s] = fMWPC_anode[s].inRange();
		fPassedCath[s] = fCathSum[s].inRange();
		fPassedCathMax[s] = fCathMax[s].inRange();			
	}
}

void ucnaDataAnalyzer11b::reconstructVisibleEnergy() {
	
	for(Side s = EAST; s <= WEST; ++s) {
		// get calibrated energy from the 4 tubes combined; also, wirechamber energy deposition estimate
		if(passedMWPC(s)) {
			PCal.calibrateEnergy(s,wirePos[s][X_DIRECTION].center,wirePos[s][Y_DIRECTION].center,sevt[s],fTimeScaler.t[BOTH]);
			fEMWPC[s] = PCal.calibrateAnode(fMWPC_anode[s].val,s,wirePos[s][X_DIRECTION].center,wirePos[s][Y_DIRECTION].center,fTimeScaler.t[BOTH]);
		} else {
			PCal.calibrateEnergy(s,0,0,sevt[s],fTimeScaler.t[BOTH]);
			fEMWPC[s] = PCal.calibrateAnode(fMWPC_anode[s].val,s,0,0,fTimeScaler.t[BOTH]);
		}
	}	
}

void ucnaDataAnalyzer11b::checkMuonVetos() {
	fTaggedTop[WEST] = false;
	fTaggedTop[EAST] = fTop_tdc[EAST].inRange();
	for(Side s = EAST; s<=WEST; ++s) {
		fTaggedBack[s] = fBacking_tdc[s].inRange();
		fTaggedDrift[s] = fDrift_tac[s].inRange();
	}
}

void ucnaDataAnalyzer11b::classifyEventType() {
	// PID
	if(isLED()) fPID = PID_LED;	// LED event identified by Sis00
	else if(isPulserTrigger()) fPID = PID_PULSER;
	else if(Is2fold(EAST) || Is2fold(WEST)) {	// passes wirechamber and scintillator cuts on either side
		if(taggedMuon())
			fPID = PID_MUON; //at least one side muon
		else fPID = PID_BETA; //beta-like
	} else fPID = PID_SINGLE; //gamma
	
	// type, side
	fType = TYPE_IV_EVENT;
	fSide = NONE;
	for(Side s = EAST; s<=WEST; ++s) {
		if(Is2fold(EAST) && Is2fold(WEST))
			fType = TYPE_I_EVENT;
		else if (Is2fold(s) && !passedCutTDC(otherSide(s)))
			fType = passedMWPC(otherSide(s))?TYPE_II_EVENT:TYPE_0_EVENT;		
		if(passedCutTDC(s)&&!passedCutTDC(otherSide(s))) fSide = s;
	}
	// if side is ambiguous, TDCW has a cleaner TDC separation; make an 1-D cut (JL)
	if(passedCutTDC(WEST)&&passedCutTDC(EAST))
		fSide = (fScint_tdc[WEST][nBetaTubes].val < ScintSelftrig[WEST].start)?EAST:WEST;
}

void ucnaDataAnalyzer11b::reconstructTrueEnergy() {
	if((fSide==EAST || fSide==WEST) && fType <= TYPE_III_EVENT)
		fEtrue = PCal.Etrue(fSide,fType,sevt[EAST].energy.x,sevt[WEST].energy.x);
	else
		fEtrue = sevt[EAST].energy.x + sevt[WEST].energy.x;
}

void ucnaDataAnalyzer11b::calcTrigEffic() {
	
	printf("\nCalculating trigger efficiency...\n");
	for(Side s = EAST; s <= WEST; ++s) {
		for(unsigned int t=0; t<nBetaTubes; t++) {
			// efficiency graph
			TGraphAsymmErrors gEffic(hTrigEffic[s][t][0]->GetNbinsX());
			gEffic.BayesDivide(hTrigEffic[s][t][1],hTrigEffic[s][t][0],"w");
			
			// scan for 50% point
			int b = gEffic.GetN();
			double midx,y;
			while(b > 1) {
				gEffic.GetPoint(--b,midx,y);
				if(y < 0.5)
					break;
			}
			
			// fit
			TF1 efficfit("efficfit",&fancyfish,-50,200,4);
			efficfit.SetParameter(0,midx);
			efficfit.SetParameter(1,10.0);
			efficfit.SetParameter(2,1.4);
			efficfit.SetParameter(3,0.99);
			efficfit.SetParLimits(0,0,100.0);
			efficfit.SetParLimits(1,2,200.0);
			efficfit.SetParLimits(2,0.1,1000.0);
			efficfit.SetParLimits(3,0.75,1.0);
			
			efficfit.SetLineColor(4);
			printf("Pre-fit threshold guess: %.1f\n",midx);
			gEffic.Fit(&efficfit,"Q");
			
			float_err trigef(efficfit.GetParameter(3),efficfit.GetParError(3));
			float_err trigc(efficfit.GetParameter(0),efficfit.GetParError(0));
			float_err trigw(efficfit.GetParameter(1),efficfit.GetParError(1));
			float_err trign_adj(efficfit.GetParameter(2),efficfit.GetParError(2));
			float trign = trigc.x/trigw.x*trign_adj.x;
			
			// save results
			printf("Poisson CDF Fit: h = %.4f(%.4f), x0 = %.1f(%.1f), dx = %.1f(%.1f), n = %.2f [adjust %.2f(%.2f)]\n",
				   trigef.x, trigef.err, trigc.x, trigc.err, trigw.x, trigw.err, trign, trign_adj.x, trign_adj.err);
			Stringmap m;
			m.insert("effic_params",vtos(efficfit.GetParameters(),efficfit.GetParameters()+4));
			m.insert("effic_params_err",vtos(efficfit.GetParErrors(),efficfit.GetParErrors()+4));
			m.insert("side",ctos(sideNames(s)));
			m.insert("tube",t);
			qOut.insert("trig_effic",m);
			// upload to analysis DB
			if(CDBout) {
				printf("Uploading trigger efficiency...\n");
				std::vector<double> tparams;
				std::vector<double> terrs;
				for(unsigned int i=0; i<4; i++) {
					tparams.push_back(efficfit.GetParameter(i));
					terrs.push_back(efficfit.GetParError(i));
				}
				CDBout->deleteTrigeff(rn,s,t);
				CDBout->uploadTrigeff(rn,s,t,tparams,terrs);
			}
			
			// plot
			gEffic.SetMinimum(-0.10);
			gEffic.SetMaximum(1.10);
			gEffic.Draw("AP");
			gEffic.SetTitle((sideSubst("%c",s)+itos(t)+" PMT Trigger Efficiency").c_str());
			gEffic.GetXaxis()->SetTitle("ADC channels above pedestal");
			gEffic.GetXaxis()->SetLimits(-50,200);
			gEffic.GetYaxis()->SetTitle("Efficiency");
			gEffic.Draw("AP");
			printCanvas(sideSubst("PMTs/TrigEffic_%c",s)+itos(t));
		}
	}
}

void ucnaDataAnalyzer11b::processEvent() {
	checkHeaderQuality();
	calibrateTimes();
	for(Side s = EAST; s <= WEST; ++s)
		PCal.pedSubtract(s, sevt[s].adc, fTimeScaler.t[BOTH]);
	fillEarlyHistograms();
	
	if(!isScintTrigger() || isLED())
		return;
	
	reconstructPosition();
	reconstructVisibleEnergy();
	checkMuonVetos();
	classifyEventType();
	reconstructTrueEnergy();
	fillHistograms();
	
	if(fPassedGlobal)
		TPhys->Fill();
}

void ucnaDataAnalyzer11b::processBiPulser() {
	
	printf("\nFitting Bi Pulser...\n");
	TF1 gausFit("gasufit","gaus",1000,4000);
	QFile pulseLocation(dataPath+"/Monitors/Run_"+itos(rn)+"/ChrisPulser.txt",false);
	for(Side s = EAST; s <= WEST; ++s) {
		for(unsigned int t=0; t<nBetaTubes; t++) {
			std::vector<double> times;
			std::vector<double> centers;
			std::vector<double> dcenters;
			std::vector<double> widths;
			std::vector<double> dwidths;
			for(unsigned int i=0; i<hBiPulser[s][t].size(); i++) {
				Stringmap m;
				m.insert("side",ctos(sideNames(s)));
				m.insert("tube",t);
				m.insert("counts",hBiPulser[s][t][i]->GetEntries());
				
				// initial estimate of peak location: first high isolated peak scanning from right
				int bmax = 0;
				float pmax = 0;
				unsigned int nskip = 0;
				for(unsigned int n = hBiPulser[s][t][i]->GetNbinsX(); n > 0; n--) {
					if(hBiPulser[s][t][i]->GetBinContent(n)>pmax) {
						bmax = n;
						pmax = hBiPulser[s][t][i]->GetBinContent(n);
						nskip = 0;
					} else {
						nskip++;
					}
					if(pmax>20 && nskip>20)
						break;
				}
				float bcenter = hBiPulser[s][t][i]->GetBinCenter(bmax);
				
				// refined fit
				if(!iterGaus(hBiPulser[s][t][i],&gausFit,3,bcenter,200,1.5)) {
					times.push_back((i+0.5)*wallTime/hBiPulser[s][t].size());
					m.insert("time",times.back());
					m.insert("height",gausFit.GetParameter(0));
					m.insert("dheight",gausFit.GetParError(0));
					centers.push_back(gausFit.GetParameter(1));
					m.insert("center",centers.back());
					dcenters.push_back(gausFit.GetParError(1));
					m.insert("dcenter",dcenters.back());
					widths.push_back(gausFit.GetParameter(2));
					m.insert("width",widths.back());
					dwidths.push_back(gausFit.GetParError(2));
					m.insert("dwidth",dwidths.back());
				}
				
				pulseLocation.insert("pulserpeak",m);
			}
			if(CDBout) {
				// upload to DB
				std::string mon_name = PCal.sensorNames[s][t];
				printf("Uploading pulser data '%s'...\n",mon_name.c_str());
				unsigned int cgid = CDBout->uploadGraph(itos(rn)+" "+mon_name+" Pulser Centers",times,centers,std::vector<double>(),dcenters);
				unsigned int wgid = CDBout->uploadGraph(itos(rn)+" "+mon_name+" Pulser Widths",times,widths,std::vector<double>(),dwidths);
				CDBout->deleteRunMonitor(rn,mon_name,"Chris_peak");
				CDBout->addRunMonitor(rn,mon_name,"Chris_peak",cgid,wgid);				
			}
			drawSimulHistos(hBiPulser[s][t]);
			printCanvas(sideSubst("PMTs/BiPulser_%c",s)+itos(t));
		}
	}
	pulseLocation.commit();
}

void ucnaDataAnalyzer11b::tallyRunTime() {
	// complete potential un-closed blip
	if(cutBlips.size() && cutBlips.back().end.t[BOTH] == 0)
		cutBlips.back().end = totalTime;
	// sum up lost time	
	BlindTime lostTime;
	for(std::vector<Blip>::iterator it = cutBlips.begin(); it != cutBlips.end(); it++)
		lostTime += it->length();
	wallTime = totalTime.t[BOTH]; // now an informed guess :) 
	totalTime -= lostTime;
	printf("\nFiducial time tally:\n");
	printf("Lost %.1fs run time to %i blips, leaving %.1fs. (%g,%g failed Evnb,Bkhf)\n",
		   lostTime.t[BOTH],(int)cutBlips.size(),totalTime.t[BOTH],nFailedEvnb,nFailedBkhf);
}

void ucnaDataAnalyzer11b::replaySummary() {
	if(!CDBout) return;
	sprintf(CDBout->query,"DELETE FROM analysis WHERE run_number = %i",rn);
	CDBout->execute();
	TDatime tNow;
	sprintf(CDBout->query,
			"INSERT INTO analysis(run_number,analysis_time,live_time_e,live_time_w,live_time,total_time,misaligned,tdc_corrupted) \
			VALUES (%i,'%s',%f,%f,%f,%f,%i,%i)",
			int(rn),tNow.AsSQLString(),totalTime.t[EAST],totalTime.t[WEST],totalTime.t[BOTH],wallTime,int(nFailedEvnb),int(nFailedBkhf));
	CDBout->execute();
	TDatime tStart(fAbsTimeStart);
	TDatime tEnd(fAbsTimeEnd);
	sprintf(CDBout->query,"UPDATE run SET start_time='%s', end_time='%s' WHERE run_number=%i",tStart.AsSQLString(),tEnd.AsSQLString(),int(rn));
	CDBout->execute();
}

void ucnaDataAnalyzer11b::quickAnalyzerSummary() const {
	printf("\n------------------ Quick Summary ------------------\n");
	float gvcounts = hMonADC[UCN_MON_GV]->Integral();
	printf("GV Mon: %i = %.2f +/- %.2f Hz\n",(int)gvcounts,gvcounts/wallTime,sqrt(10*gvcounts)/10/wallTime);
	float fecounts = hMonADC[UCN_MON_FE]->Integral();
	printf("Fe Mon: %i = %.2f Hz; Fe/GV = %.4f\n",(int)fecounts,fecounts/wallTime,fecounts/gvcounts);
	float scscounts = hMonADC[UCN_MON_SCS]->Integral();
	printf("SCS Mon: %i = %.2f Hz; SCS/GV = %.4f\n",(int)scscounts,scscounts/wallTime,scscounts/gvcounts);
	float scounts[2];
	for(Side s = EAST; s <= WEST; ++s) {
		scounts[s] = hEtrue[s][TYPE_0_EVENT]->Integral()+hEtrue[s][TYPE_I_EVENT]->Integral()+hEtrue[s][TYPE_II_EVENT]->Integral();
		printf("%s Beta Triggers: %i = %.2f +/- %.2f Hz\n",sideWords(s),(int)scounts[s],scounts[s]/wallTime,sqrt(scounts[s])/wallTime);
	}
	printf("Beta/GV = %.2f\n",(scounts[EAST]+scounts[WEST])/gvcounts);
	printf("Bonehead (E-W)/(E+W) = %.2f%%\n",100.0*(scounts[EAST]-scounts[WEST])/(scounts[EAST]+scounts[WEST]));
	TDatime stime;
	stime.Set(fAbsTime);
	printf("----------------------------------------------------\n");
	printf("%s\t%i\t%i\t%i\t%.1f\n",stime.AsSQLString(),rn,(int)scounts[EAST],(int)scounts[WEST],wallTime);
	printf("----------------------------------------------------\n\n");
}

int main(int argc, char** argv) {
	
	// check correct arguments
	if(argc<2) {
		printf("Syntax: %s <run number> [ignore beam/source status]\n",argv[0]);
		exit(1);
	}
	
	// get run(s)
	std::vector<int> rlist = sToInts(argv[1],"-");
	if(!rlist.size() || !rlist[0] || rlist.size()>2) {
		printf("*** '%s' is not a valid run number! Exiting!\n",argv[1]);
		exit(1);
	}
	if(rlist.size()==1)
		rlist.push_back(rlist[0]);
	
	// other options
	bool cutBeam = false;
	bool nodbout = false;
	bool noroot = false;
	for(int i=2; i<argc; i++) {
		std::string arg(argv[i]);
		if(arg=="cutbeam")
			cutBeam = true;
		else if(arg=="nodbout")
			nodbout = true;
		else if(arg=="noroot")
			noroot = true;
		else
			assert(false);
	}
	
	gStyle->SetPalette(1);
	gStyle->SetNumberContours(255);
	gStyle->SetOptStat("e");
	
	std::string outDir = getEnvSafe("UCNAOUTPUTDIR");
	
	for(RunNum r = (unsigned int)rlist[0]; r<=(unsigned int)rlist[1]; r++) {
		
		std::string inDir = getEnvSafe("UCNADATADIR");
		if(!fileExists(inDir+"/full"+itos(r)+".root") && r > 16300)
			inDir = "/data/ucnadata/2011/rootfiles/";
		
		ucnaDataAnalyzer11b A(r,outDir,CalDBSQL::getCDB(true));
		A.setIgnoreBeamOut(!cutBeam);
		if(!nodbout) {
			printf("Connecting to output DB...\n");
			A.setOutputDB(CalDBSQL::getCDB(false));
		}
		A.addFile(inDir+"/full"+itos(r)+".root");
		A.analyze();
		A.setWriteRoot(!noroot);
		A.write();
	}
	
	return 0;
}
