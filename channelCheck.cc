#include <iostream>
#include <map>
#include "glob.h"
#include "TFile.h"
#include "MJTRun.hh"
#include "MJTChannelMap.hh"
#include "MJTChannelSettings.hh"
#include "GATDataSet.hh"
#include "GATDetInfoProcessor.hh"
#include "GATChannelSelectionInfo.hh"
#include "DataSetInfo.hh"
using namespace std;

int main()
{
  int dsNum = 0;

  vector<int> runList;
  GATDataSet ds;
  for (int rs = 0; rs <= GetDataSetSequences(dsNum); rs++) LoadDataSet(ds, dsNum, rs);
  // LoadDataSet(ds, dsNum, 29); // just the bad range, 42
  for (size_t i = 0; i < ds.GetNRuns(); i++) runList.push_back(ds.GetRunNumber(i));

  for (auto run: runList)
    cout << run << ", ";
  cout << endl;
  return 0;

  map<int,bool> detIDIsBad = LoadBadDetectorMap(dsNum);
  map<int,bool> detIDIsVetoOnly = LoadVetoDetectorMap(dsNum);
  map <int,int> detChanToDetIDMap;

  double runTime672=0, runTime673=0;
  for (auto run: runList)
  {
    GATDataSet ds;
    string bltPath = ds.GetPathToRun(run,GATDataSet::kBuilt);
    TFile *bltFile = new TFile(bltPath.c_str());

    // runtime
    double start=0, stop=0, thisRunTime=0;
    MJTRun *runInfo = (MJTRun*)bltFile->Get("run");
    start = runInfo->GetStartClockTime();
    stop = runInfo->GetStopClockTime();
    thisRunTime = (stop-start)/1e9;
    if(thisRunTime < 0) {
      start = runInfo->GetStartTime();
      stop = runInfo->GetStopTime();
      thisRunTime = (stop-start);
      printf("Reverting to the unix timestamps (%.2f) for run %d \n",thisRunTime,run);
    }

    // enabled channels
    MJTChannelMap *chMap = (MJTChannelMap*)bltFile->Get("ChannelMap");
    MJTChannelSettings *chSet = (MJTChannelSettings*)bltFile->Get("ChannelSettings");
    vector<uint32_t> enabledIDs = chSet->GetEnabledIDList();

    bool en672 = false, en673 = false;
    if ( find(enabledIDs.begin(), enabledIDs.end(), 672) != enabledIDs.end() ) en672=true;
    if ( find(enabledIDs.begin(), enabledIDs.end(), 673) != enabledIDs.end() ) en673=true;

    // bad/veto-only maps
    vector<uint32_t>::iterator ite = enabledIDs.begin();
    while (ite != enabledIDs.end()){
      int enab = *ite;
      GATDetInfoProcessor gp;
      int detID = gp.GetDetIDFromName( chMap->GetString(enab, "kDetectorName") );
      detChanToDetIDMap[enab] = detID;
      if (detIDIsVetoOnly[detID]) { enabledIDs.erase(ite); continue; } // erase increments the iterator
      else if (detIDIsBad[detID]) { enabledIDs.erase(ite); continue; }
      else ++ite;
    }

    // chanel selection files
    bool isVetoDet=false, isBadDet=false;
    string chSelPath = GetChannelSelectionPath(dsNum);
    if (FILE *file = fopen(chSelPath.c_str(), "r")) {
      fclose(file);
      GATChannelSelectionInfo ch_select (chSelPath, run);
      vector<int> DetIDList = ch_select.GetDetIDList();
      for (size_t ich=0; ich < DetIDList.size(); ich++)
      {
        int detID = DetIDList[ich];
        pair<int,int> ch_pair = ch_select.GetChannelsFromDetID(detID);

        cout << run << " " << ch_pair.first << " " << ch_pair.second << " " << detID << endl;

        isVetoDet = (ch_select.GetDetIsVetoOnly(detID));
        isBadDet = (ch_select.GetDetIsBad(detID));

        // If the channel is flagged, remove it from the enabledIDs.
        if (isVetoDet || isBadDet) {
          cout << "True for channel " << ch_pair.first << endl;
          uint32_t hgChan = (uint32_t)ch_pair.first;
          auto it = std::find(enabledIDs.begin(), enabledIDs.end(), hgChan);
          if (it != enabledIDs.end()) enabledIDs.erase(it);

          uint32_t lgChan = (uint32_t)ch_pair.second;
          it = std::find(enabledIDs.begin(), enabledIDs.end(), lgChan);
          if (it != enabledIDs.end()) enabledIDs.erase(it);
        }
      }
    }

    bool sel672=false, sel673=false;
    if ( find(enabledIDs.begin(), enabledIDs.end(), 672) == enabledIDs.end() ) {
      sel672=true;
    }
    if ( find(enabledIDs.begin(), enabledIDs.end(), 673) == enabledIDs.end() ) {
      sel673=true;
    }

    cout << Form("%-6i  %-6.1f  en672? %i  en673? %i  sel672? %i  sel673? %i  isVeto %i  isBad %i\n", run,thisRunTime,en672,en673,sel672,sel673,isVetoDet,isBadDet);

    if (en672 && !sel672) runTime672 += thisRunTime;
    if (en673 && !sel673) runTime673 += thisRunTime;

    delete bltFile;
  }

  cout << "Runtime results: 672: " << runTime672/86400. << "  673: " << runTime673/86400. << endl;


}