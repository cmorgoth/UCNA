#include "SegmentSaver.hh"
#include "Types.hh"

TH1* SegmentSaver::registerSavedHist(const std::string& hname, const std::string& title,unsigned int nbins, float xmin, float xmax) {
	assert(saveHists.find(hname)==saveHists.end());	// don't duplicate names!
	TH1* h;
	if(fIn)
		h = (TH1*)addObject(fIn->Get(hname.c_str())->Clone(hname.c_str()));
	else
		h = registeredTH1F(hname,title,nbins,xmin,xmax);
	saveHists.insert(std::make_pair(hname,h));
	return h;
}

TH1* SegmentSaver::registerSavedHist(const std::string& hname, const TH1& hTemplate) {
	assert(saveHists.find(hname)==saveHists.end());	// don't duplicate names!
	TH1* h;
	if(fIn) {
		h = (TH1*)addObject(fIn->Get(hname.c_str())->Clone(hname.c_str()));
	} else {
		h = (TH1*)addObject(hTemplate.Clone(hname.c_str()));
		zero(h);
	}
	saveHists.insert(std::make_pair(hname,h));
	return h;
}

SegmentSaver::SegmentSaver(OutputManager* pnt, const std::string& nm, const std::string& inflName):
OutputManager(nm,pnt), inflname(inflName) {		
	// open file to load existing data
	fIn = (inflname.size())?(new TFile((inflname+".root").c_str(),"READ")):NULL;
	assert(!fIn || !fIn->IsZombie());
	if(fIn)
		printf("Loading data from %s...\n",inflname.c_str());
}

SegmentSaver::~SegmentSaver() {
	if(fIn) {
		fIn->Close();
		delete(fIn);
	}
}

TH1* SegmentSaver::getSavedHist(const std::string& hname) {
	std::map<std::string,TH1*>::iterator it = saveHists.find(hname);
	assert(it != saveHists.end());
	return it->second;
}

const TH1* SegmentSaver::getSavedHist(const std::string& hname) const {
	std::map<std::string,TH1*>::const_iterator it = saveHists.find(hname);
	assert(it != saveHists.end());
	return it->second;
}

void SegmentSaver::zeroSavedHists() {
	for(std::map<std::string,TH1*>::iterator it = saveHists.begin(); it != saveHists.end(); it++)
		zero(it->second);
}

bool SegmentSaver::isEquivalent(const SegmentSaver& S) const {
	if(saveHists.size() != S.saveHists.size()) return false;
	for(std::map<std::string,TH1*>::const_iterator it = saveHists.begin(); it != saveHists.end(); it++) {
		std::map<std::string,TH1*>::const_iterator otherit = S.saveHists.find(it->first);
		if(otherit == S.saveHists.end()) return false;
		// TODO other checks?
	}
	return true;
}

void SegmentSaver::addSegment(const SegmentSaver& S) {
	assert(isEquivalent(S));
	// add histograms
	for(std::map<std::string,TH1*>::const_iterator it = saveHists.begin(); it != saveHists.end(); it++)
		it->second->Add(S.getSavedHist(it->first));
}
