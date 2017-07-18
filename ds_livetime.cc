// MJD Data Set livetime & exposure calculator.
// C. Wiseman, USC
// S. Meijer and A. Reine, UNC

#include <iostream>
#include <map>
#include <ctime>
#include <sstream>
#include <iterator>
#include "TFile.h"
#include "TChain.h"
#include "TTreeReader.h"
#include "TTreeReaderArray.h"
#include "MJVetoEvent.hh"
#include "MJTRun.hh"
#include "GATDataSet.hh"
#include "GATChannelSelectionInfo.hh"
#include "GATDetInfoProcessor.hh"
#include "DataSetInfo.hh"

// TODO / FIXME: For the db2 method, where we don't have a subrange number,
//               how to add/lookup the correct deadtime information?

// NOTE:  Right now this will double-count an overlapping veto+LN fill.
//        If we add more effects it will double count them too.
//        If we want an exact, non-overlapping calculation of dead time,
//        we need to modify the function mergeIntervals to accept a list
//        of every veto period in a run. Its algorithm should be able
//        to exactly calculate the dead time.
//        Until someone is ready to do this, let's just report the
//        reductions as fractions from the raw livetime.

/*
  Results, 13 June 2017 (DataSetInfo.hh from this date.  db2 method used.)
  DS    Livetime   Enr. Act. Mass  Enr. Exp.      Nat. Act. Mass  Nat. Exp.
  DS0   46.0368    10.104          478.915        3.905           185.092
  DS1   58.93      11.3102         661.523        1.121           66.0583
  DS2   9.65746    11.3102         109.224        1.121           10.8256
  DS3   29.9112    12.0402         360.124        2.781           83.18
  DS4   23.6842    5.4712          129.578        3.95            93.5506
  DS5M1 121.779    12.0402         1436.07        3.912           452.974
  DS5M2 121.779    6.152           693.184        5.085           548.364

  Results, 13 May 2017 (Used for LEGEND Meeting)
  DS    Livetime   Enr. Act. Mass  Enr. Exp.      Nat. Act. Mass  Nat. Exp.
  DS0   47.4283    10.104          479.216        3.905           185.208
  DS1   58.9347    11.310          661.597        1.121           66.0657
  DS2   9.6594     11.310          109.25         1.121           10.828
  DS3   29.9286    12.040          360.346        2.781           83.231
  DS4   23.7788    5.471           130.098        3.95            93.926
  DS5M1 122.557    12.040          1445.23        3.912           455.998
  DS5M2 122.557    6.152           697.178        5.085           552.012
*/

using namespace std;
using namespace MJDB;

void calculateLiveTime(vector<int> runList, vector<pair<int,double>> times, int dsNum, bool raw, bool runDB,
  map<int,vector<int>> burst, map<int,vector<int>> ranges = map<int,vector<int>>());
bool compareInterval(pair<int,int> i1, pair<int,int> i2) { return (i1.first < i2.first); }
int mergeIntervals(vector<pair<int,int>> vals, int start, int stop);
void getDBRunList(int &dsNum, double &ElapsedTime, string options, vector<int> &runList, vector<pair<int,double>> &times);
void locateRunRange(int run, map<int,vector<int>> ranges, int& runInSet, int& firstRunInSet);
map<int, vector<int>> getDeadtimeRanges(int dsNum) ;

int main(int argc, char** argv)
{
  // "let's get some (m)args"
  if (argc < 2) {
		cout << "Usage: ./ds_livetime [dsNum] [options]\n"
         << " Options:\n"
         << "   -raw: Only get raw duration\n"
         << "   -gds: Get livetime from GATDataSet\n"
         << "   -db1 ['options in quotes']: Get run list from runDB and quit\n"
         << "   -db2 ['options in quotes']: Do full LT calculation on a runDB list\n"
         << "   -low: GDS method + low energy run/channel selection list.\n"
         << " RunDB access (-db[12] option):\n"
         << "    partNum = P3LQK, P3KJR, P3LQG, etc.\n"
         << "    runRank = gold, silver, bronze, cal, etc.\n"
         << "    dataset = 0 thru 6\n"
         << "    Ex.1: ./ds_livetime 5 -db1 'runrank P3KJR silver' (note the single quote)\n"
         << "    Ex.2: ./ds_livetime 5 -db1 'dataset 3'\n";
		return 1;
	}
  int dsNum = stoi(argv[1]);
  string runDBOpt = "";
  bool raw=0, gds=0, lt=1, rdb=0, low=0;
  map<int,vector<int>> burst;
  vector<string> opt(argv+1, argv+argc);
  for (size_t i = 0; i < opt.size(); i++) {
    if (opt[i] == "-raw") { raw=1; }
    if (opt[i] == "-gds") { gds=1; }
    if (opt[i] == "-db1") { lt=0; rdb=1; runDBOpt = opt[i+1]; }
    if (opt[i] == "-db2") { lt=1; rdb=1; runDBOpt = opt[i+1]; }
    if (opt[i] == "-low") { lt=0; low=1; }
  }

  // Do GATDataSet method and quit (-gds)
  if (gds) {
    cout << "Scanning DS-" << dsNum << " with GetRunTime...\n";
    GATDataSet gds;
    for (int rs = 0; rs <= GetDataSetSequences(dsNum); rs++) LoadDataSet(gds, dsNum, rs);
    cout << Form("DS-%i total from GetRunTime: %.4f days.\n",dsNum,gds.GetRunTime()/1e9/86400);
    return 0;
  }

  // Do RunDB method and quit (-db1)
  if (!lt && rdb) {
    double ElapsedTime;
    vector<int> runList;
    vector<pair<int,double>> times;
    getDBRunList(dsNum, ElapsedTime, runDBOpt, runList, times);
    // for (size_t i = 0; i < times.size(); i++) {
    //   cout << times[i].first << " " << times[i].second << endl;
    // }
    cout << Form("DS-%i total from RunDB: %.4f days.\n",dsNum,ElapsedTime/86400);
    return 0;
  }

  // Primary livetime routine, using DataSetInfo run sequences (default, no extra args)
  if (lt && !rdb) {
    GATDataSet ds;
    cout << "Scanning DS-" << dsNum << endl;

    // Method 1: get ranges from DataSetInfo
    // map<int, vector<int>> ranges = LoadDataSet(ds, dsNum, 0);
    // for (int rs = 1; rs <= GetDataSetSequences(dsNum); rs++) LoadDataSet(ds, dsNum, rs);

    // Method 2: get ranges from deadtime '.lis' files.
    for (int rs = 0; rs <= GetDataSetSequences(dsNum); rs++) LoadDataSet(ds, dsNum, rs);
    map<int, vector<int>> ranges = getDeadtimeRanges(dsNum);

    // DEBUG: Print the run ranges from DataSetInfo
    for (auto& r : ranges) {
      cout << r.first << " ";
      for (auto run : r.second) cout << run << " ";
      cout << endl;
    }

    vector<int> runList;
    for (int i = 0; i < (int)ds.GetNRuns(); i++) runList.push_back(ds.GetRunNumber(i));

    vector<pair<int,double>> times;
    calculateLiveTime(runList,times,dsNum,raw,rdb,burst,ranges);
  }

  // Do primary livetime routine using a run list from the RunDB (-db2)
  // TODO / FIXME: How to add the deadtime information?
  if (lt && rdb) {
    double ElapsedTime=0;
    vector<int> runList;
    vector<pair<int,double>> times;
    getDBRunList(dsNum, ElapsedTime, runDBOpt, runList, times); // uses "times", auto-detects dsNum
    calculateLiveTime(runList,times,dsNum,raw,rdb,burst);
  }

  // Do primary livetime with a low-energy burst cut applied.
  if (low) {
    cout << "Scanning DS-" << dsNum << endl;

    GATDataSet ds;
    map<int, vector<int>> ranges = LoadDataSet(ds, dsNum, 0);
    for (int rs = 1; rs <= GetDataSetSequences(dsNum); rs++) LoadDataSet(ds, dsNum, rs);

    vector<int> runList;
    for (int i = 0; i < (int)ds.GetNRuns(); i++) runList.push_back(ds.GetRunNumber(i));

    cout << "Loading burst cut ...\n";
    map<int,vector<int>> burst;
    ifstream inFile;
    inFile.open("burstCut_v1.txt");
    string line;
    while (getline(inFile, line)) {
      vector<string> inputs;
      istringstream iss(line);
      string buf;
      while (iss >> buf) inputs.push_back(buf);
      int key = stoi(inputs[0]);
      vector<int> cut;
      for (size_t i = 1; i < inputs.size(); i++) cut.push_back( stoi(inputs[i]) );
      burst[ key ] = cut;
    }
    // Also note channels that were entirely removed from the dataset.
    burst[0] = {656};
    burst[3] = {592,692};
    burst[4] = {1332};
    burst[5] = {614,692,1124,1232};

    vector<pair<int,double>> times;
    calculateLiveTime(runList,times,dsNum,raw,rdb,burst);
  }
}


void calculateLiveTime(vector<int> runList, vector<pair<int,double>> times, int dsNum, bool raw, bool runDB,
  map<int,vector<int>> burst, map<int,vector<int>> ranges)
{
  // Do we have M1 and M2 enabled?
  bool mod1=0, mod2=0;
  if (dsNum==0 || dsNum==1 || dsNum==2 || dsNum==3) { mod1=1; mod2=0; }
  else if (dsNum == 4)  { mod1=0; mod2=1; }
  else if (dsNum == 5)  { mod1=1; mod2=1; }

  // Are we applying a burst cut?
  bool useBurst=0;
  if (burst.size() > 0) {
    cout << "Applying burst cut...\n";
    useBurst=1;
  }

  // Load LN fill times
  double loFill = 15*60, hiFill = 5*60; // veto window
  vector<double> lnFillTimes[2];
  if (mod1) LoadLNFillTimes1(lnFillTimes[0], dsNum);
  if (mod2) LoadLNFillTimes2(lnFillTimes[1], dsNum);

  // Load detector maps
  map<int,bool> detIDIsBad = LoadBadDetectorMap(dsNum);
  map<int,bool> detIDIsVetoOnly = LoadVetoDetectorMap(dsNum);
  map<int,double> actM4Det_g = LoadActiveMasses(dsNum);
  map<int,bool> detIsEnr = LoadEnrNatMap();

  // Start loop over runs.
  double rawLive=0, vetoLive=0, vetoDead=0, m1LNDead=0, m2LNDead=0;
  map <int,double> channelRuntime, channelLivetime, channelLivetimeML;
  map <int,int> detChanToDetIDMap;
  map<string, vector<double>> dtMap;
  time_t prevStop=0;
  int prevSubSet=-1;
  for (size_t r = 0; r < runList.size(); r++)
  {
    int run = runList[r];
    if ((int)(100*(double)r/runList.size())%10==0)
      cout << 100*(double)r/runList.size() << "% done\n";

    cout << "Scanning run " << run << endl;

    // locate subset and first run in set numbers.
    int runInSet, firstRunInSet;
    locateRunRange(run,ranges,runInSet,firstRunInSet);

    // Load the deadtime file ONLY when the subset changes and repopulate the deadtime map 'dtMap'.
    if (runInSet != prevSubSet)
    {
      cout << "Subset " << runInSet << ", loading DT file.\n";

      string dtFilePath = Form("./deadtime/ds%i_%i.DT",dsNum,runInSet);
      if (dsNum==5) dtFilePath = Form("./deadtime/DS%i.DT",firstRunInSet);
      ifstream dtFile(dtFilePath.c_str());
      if (!dtFile) {
        cout << "Couldn't find file: " << dtFilePath << endl;
        return;
      }
      for (int i = 0; i < 2; i++) dtFile.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

      string buffer;
      dtMap.clear();
      while (getline(dtFile, buffer))
      {
        int id, pos, p1, p2, p3, p4, p5, p6;
        double hgFWHM, hgNeg, hgPos, hgDead;
        double lgFWHM, lgNeg, lgPos, lgDead;
        double orDead;
        string det;

        // TODO: if david's code gives "nan" values, they'll be read as strings.
        // fix it like here: https://stackoverflow.com/questions/24504582/how-to-test-whether-stringstream-operator-has-parsed-a-bad-type-and-skip-it

        istringstream iss(buffer);
        iss >> id >> pos >> hgFWHM >> hgNeg >> hgPos >> hgDead
            >> lgFWHM >> lgNeg >> lgPos >> lgDead >> orDead
            >> det >> p1 >> p2 >> p3 >> p4 >> p5 >> p6;
        cout << Form("%i %i %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %s %i %i %i %i %i %i\n" ,id,pos,hgFWHM,hgNeg,hgPos,hgDead,lgFWHM,lgNeg,lgPos,lgDead,orDead,det.c_str(),p1,p2,p3,p4,p5,p6);

        // Check if anything is nan.  We'll take it to mean 100% dead.
        if(hgDead != hgDead) hgDead = 1.0;
        if(lgDead != lgDead) lgDead = 1.0;
        if(orDead != orDead) orDead = 1.0;

        // fill the deadtime map
        dtMap[det] = {hgDead,lgDead,orDead,(double)p1,(double)p2,(double)p3};
      }

      // Save the subset number
      prevSubSet = runInSet;
    }

    // Load built file
    GATDataSet ds;
    string bltPath = ds.GetPathToRun(run,GATDataSet::kBuilt);
    TFile *bltFile = new TFile(bltPath.c_str());

    // Get start/stop time, and add to raw live time
    time_t start=0, stop=0;
    if (runDB) {
      rawLive += times[r].second;
      stop = times[r].first;
      start = times[r].first - (int)times[r].second;
    }
    else {
      MJTRun *runInfo = (MJTRun*)bltFile->Get("run");
      start = runInfo->GetStartTime();
      stop = runInfo->GetStopTime();
      struct tm *tmStart, *tmStop;  // I dunno if this is the best way to check for bad start/stop vals
      tmStart = gmtime(&start), tmStop = gmtime(&stop);
      int yrStart = 1900+tmStart->tm_year, yrStop = 1900+tmStop->tm_year;
      if (yrStart < 2005 || yrStart > 2025 || yrStop < 2005 || yrStart > 2025) {
        cout << Form("Run %i has corrupted start/stop packets.  Start (yr%i) %li  Stop (yr %i) %li.  Continuing...\n", run,yrStart,start,yrStop,stop);
        continue;
      }
      rawLive += (double)(stop - start);
    }
    if (raw) {
      delete bltFile;
      continue;
    }

    // Get veto system livetime and deadtime.
    double vetoDeadRun=0;
    if (dsNum!=4)
    {
      string vetPath = ds.GetPathToRun(run,GATDataSet::kVeto);
      if (FILE *file = fopen(vetPath.c_str(), "r")) {
        fclose(file);

        // Load tree and find veto livetime
        TFile *vetFile = new TFile(vetPath.c_str());
        TTree *vetTree = (TTree*)vetFile->Get("vetoTree");
        TTreeReader vReader(vetTree);
        TTreeReaderValue<MJVetoEvent> vetoEventIn(vReader,"vetoEvent");
        TTreeReaderValue<Long64_t> vetoStart(vReader,"start");
        TTreeReaderValue<Long64_t> vetoStop(vReader,"stop");
        TTreeReaderValue<double> timeUncert(vReader,"timeUncert");
        TTreeReaderArray<int> CoinType(vReader,"CoinType");
        vReader.SetEntry(0);
        long long vStart=0, vStop=0;
        vStart = (*vetoStart);
        vStop = (*vetoStop);
        if (runDB) {
          vetoLive += times[r].second; // use duration from runDB
          vStart = start; // use same duration as built files (this is OK)
          vStop = stop;
        }
        else vetoLive += (double)(vStop-vStart);
        vReader.SetTree(vetTree);  // resets the reader

        // Find veto deadtime
        bool newRun=1;
        while(vReader.Next()) {
          int idx = vReader.GetCurrentEntry();
          if (idx!=0) newRun=0;
          MJVetoEvent veto = *vetoEventIn;
          int type=0;
          if (CoinType[0]) type=1;
          if (CoinType[1]) type=2; // overrides type 1 if both are true
          if ((time_t)vStart-prevStop > 10 && newRun) type = 3; // in case we missed a muon in a run gap
          if (type!=0) {
            if (veto.GetBadScaler()) *timeUncert = 8.0; // fix uncertainty for corrupted scalers
            vetoDeadRun += 1. + 2 * fabs(*timeUncert); // matches muVeto window in skim_mjd_data
            // cout << Form("Run %i  type %i  uncert %-6.4f  vetoDeadRun %-6.4f  vetoDead %-6.4f\n", run,type,*timeUncert,vetoDeadRun,vetoDead);
          }
        }
        delete vetFile;
      }
    }
    else if (dsNum==4) {
      vector<int> muRuns, muTypes;
      vector<double> muRunTStarts, muTimes, muUncert;
      LoadDS4MuonList(muRuns,muRunTStarts,muTimes,muTypes,muUncert);

      vector<int> idxThisRun;
      for (size_t i = 0; i < muRuns.size(); i++)
        if (muRuns[i]==run) idxThisRun.push_back(i);

      for (size_t i = 0; i < idxThisRun.size(); i++) {
        int muIdx = idxThisRun[i];
        vetoDeadRun += 4 + 4. * fabs(muUncert[muIdx]); // matches muVeto window in skim_mjd_data
        // cout << Form("Run %i  type %i  uncert %-6.4f  vetoDeadRun %-6.4f  vetoDead %-6.4f\n", run,muTypes[muIdx],muUncert[muIdx],vetoDeadRun,vetoDead);
      }
    }
    prevStop = stop;
    vetoDead += vetoDeadRun;

    // Calculate LN fill deadtime.
    int m1LNDeadRun=0, m2LNDeadRun=0;
    // Find all fills whose veto window falls within this run.
    vector<pair<int,int>> runFills[2];
    for (int mod = 0; mod < 2; mod++) {
      for (auto fill : lnFillTimes[mod])
        if ((fill > start && fill < stop) ||         // fill within this run
           (fill < start && fill+hiFill > start) ||  // < 5 mins before this run
           (fill > stop && fill-loFill < stop))      // < 15 mins after this run
           runFills[mod].push_back(make_pair(fill-loFill,fill+hiFill));
    }
    if (mod1 && runFills[0].size() > 0) m1LNDeadRun = mergeIntervals(runFills[0],start,stop);
    if (mod2 && runFills[1].size() > 0) m2LNDeadRun = mergeIntervals(runFills[1],start,stop);
    // if (m1LNDeadRun > 0 || m2LNDeadRun > 0) cout << Form("Fill: Run %i  mod1: %i  mod2 %i\n",run,m1LNDeadRun,m2LNDeadRun);
    m1LNDead += m1LNDeadRun;
    m2LNDead += m2LNDeadRun;

    // Calculate each enabled detector's livetime.
    MJTChannelMap *chMap = (MJTChannelMap*)bltFile->Get("ChannelMap");
    MJTChannelSettings *chSet = (MJTChannelSettings*)bltFile->Get("ChannelSettings");
    vector<uint32_t> enabledIDs = chSet->GetEnabledIDList();
    vector<uint32_t> pulserMons = chMap->GetPulserChanList();

    // Apply the DataSetInfo veto-only and bad lists regardless of having a channel selection object.
    // Don't count the livetime from detectors that are on these lists.
    // Also, save the channel to detID map for the exposure calculation.
    // Look for a GATChannelSelectionInfo file and remove the enabledID if it's flagged as bad.
    // If we're applying a burst cut for this run, remove the affected channels.

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

    // Now try and load a channel selection object and pop any other bad detectors
    string chSelPath = Form("/global/projecta/projectdirs/majorana/users/jwmyslik/analysis/channelselection/DS%i/v_20170510-00001",dsNum);
    if (FILE *file = fopen(chSelPath.c_str(), "r")) {
      fclose(file);
      GATChannelSelectionInfo ch_select (chSelPath, run);
      vector<int> DetIDList = ch_select.GetDetIDList();
      for (size_t ich=0; ich < DetIDList.size(); ich++)
      {
        int detID = DetIDList[ich];
        pair<int,int> ch_pair = ch_select.GetChannelsFromDetID(detID);
        bool isVetoDet = (ch_select.GetDetIsVetoOnly(detID));
        bool isBadDet = (ch_select.GetDetIsBad(detID));

        // If the channel is flagged, remove it from the enabledIDs.
        if (isVetoDet || isBadDet) {
          uint32_t hgChan = (uint32_t)ch_pair.first;
          auto it = std::find(enabledIDs.begin(), enabledIDs.end(), hgChan);
          if (it != enabledIDs.end()) enabledIDs.erase(it);

          uint32_t lgChan = (uint32_t)ch_pair.second;
          it = std::find(enabledIDs.begin(), enabledIDs.end(), lgChan);
          if (it != enabledIDs.end()) enabledIDs.erase(it);
        }
      }
    }

    // Now apply the burst cut
    if (useBurst)
    {
      // make a list of channels to remove
      vector<int> killChs;

      // search for the dataset-wide channels to remove
      map<int,vector<int>>::iterator it = burst.find(dsNum);
      if(it != burst.end())
        for (auto ch : it->second)
          killChs.push_back(ch);

      // search for the run-specific channels to remove
      map<int,vector<int>>::iterator it2 = burst.find(run);
      if(it2 != burst.end())
        for (auto ch : it2->second)
          killChs.push_back(ch);

      // now remove the channels
      for (auto ch : killChs) {
        auto it3 = std::find(enabledIDs.begin(), enabledIDs.end(), ch);
        if (it3 != enabledIDs.end()) {
          enabledIDs.erase(it3);
          cout << Form("DS %i  run %i  removed %i . enabled: ",dsNum,run,ch);
        }
      }
      for (auto ch : enabledIDs) cout << ch << " ";
      cout << endl;
    }

    // Finally, add to the raw and reduced livetime of ONLY GOOD detectors.
    for (auto ch : enabledIDs)
    {
      // don't include pulser monitors.
      // if ( find(pulserMons.begin(), pulserMons.end(), ch) != pulserMons.end() ) continue;
      if (detChanToDetIDMap[ch] == -1) continue;

      // Runtime
      channelRuntime[ch] += (double)(stop-start); // creates new entry if one doesn't exist

      // Livetime (contains deadtime correction)
      string pos = chMap->GetDetectorPos(ch);
      if (dtMap.find(pos) != dtMap.end())
      {
        double hgDead = dtMap[pos][0]/100.0; // value is in percent, divide by 100
        double lgDead = dtMap[pos][1]/100.0;
        double orDead = dtMap[pos][2]/100.0;
        double hgPulsers = dtMap[pos][3];
        double lgPulsers = dtMap[pos][4];
        double orPulsers = dtMap[pos][5];

        // TODO: This converts any "-9.99" into a 0 deadtime, but we should
        // probably actually use the average value for the subset.
        if (hgDead < 0) hgDead = 0;
        if (lgDead < 0) lgDead = 0;
        if (orDead < 0) orDead = 0;

        // hgDead = (1. - hgDead);
        // lgDead = (1. - lgDead);
        // orDead = (1. - orDead);

        // TODO: Make this more general.
        // The following assumes only DS2 uses presumming, and may not always be true
        // Takes out 62 or 100 µs per pulser as deadtime
        double hgPulserDT = hgPulsers*(dsNum==2?100e-6:62e-6);
        double lgPulserDT = lgPulsers*(dsNum==2?100e-6:62e-6);
        double orPulserDT = orPulsers*(dsNum==2?100e-6:62e-6);

        // Livetime
        if (ch%2 == 0) {
          channelLivetime[ch] += (double)(stop-start) * (1 - hgDead);
          // printf("   livetime[%d]: %f   ,%.3f  *   (1 - %f)\n",ch,channelLivetime[ch],(double)(stop-start), hgDead);
        }
        if (ch%2 == 1){
          channelLivetime[ch] += (double)(stop-start) * (1 - lgDead);
          // printf("   livetime[%d]: %f   ,%.3f  *   (1 - %f)\n",ch,channelLivetime[ch],(double)(stop-start), lgDead);
        }

        // Remove some for the pulser deadtime
        if (ch%2 == 0) channelLivetime[ch] -= hgPulserDT;
        if (ch%2 == 1) channelLivetime[ch] -= lgPulserDT;

        // TODO: we need an object with one entry for every DETECTOR, not channel.
        // Maybe the best way to do that is to form it from "channelLivetimeML" AFTER this loop.
        channelLivetimeML[ch] += (double)(stop-start) * (1 - orDead);
        channelLivetimeML[ch] -= orPulserDT;
      }
      else {
        // This means that a detector was in the channel map, but not David's file?
        cout << "Warning: Detector " << pos << " not found! Exiting ...\n";
        return;
      }

      // LN reduction - depends on if channel is M1 or M2
      GATDetInfoProcessor gp;
      int detID = gp.GetDetIDFromName( chMap->GetString(ch, "kDetectorName") );
      if (CheckModule(detID)==1) {
        channelLivetime[ch] -= m1LNDeadRun;
        channelLivetimeML[ch] -= m1LNDeadRun;
      }
      if (CheckModule(detID)==2) {
        channelLivetime[ch] -= m2LNDeadRun;
        channelLivetimeML[ch] -= m2LNDeadRun;
      }

      // Veto reduction - applies to all channels in BOTH modules.
      channelLivetime[ch] -= vetoDeadRun;
      channelLivetimeML[ch] -= vetoDeadRun;
    }

    // Done with this run.
    delete bltFile;
  }

  // Calculate channel-by-channel exposure in kg-days
  // 86400 seconds = 1 day
  rawLive = rawLive/86400;
  vetoLive = vetoLive/86400;
  vetoDead = vetoDead/86400;
  m1LNDead = m1LNDead/86400;
  m2LNDead = m2LNDead/86400;
  for (auto &raw : channelRuntime) raw.second = raw.second/86400;
  for (auto &live : channelLivetime) live.second = live.second/86400;


  // TODO: implement 3 livetimes: HG, LG, and "either".  How you wanna do this??
  // Reminder:  you're working with the maps "channelLivetime" (should contain HG and LG),
  // and "channelLivetimeML", which should contain "either"


  double m1EnrExp=0, m1NatExp=0, m2EnrExp=0, m2NatExp=0;
  double m1EnrActMass=0, m1NatActMass=0, m2EnrActMass=0, m2NatActMass=0;
  map <int,double> channelExposure;
  for (auto &live : channelLivetime) // comment this back in for an exact calculation
  // for (auto &raw: channelRuntime) // for now, just use the runtime number in the exposure.
  {
    // int chan = raw.first;
    // double livetime = raw.second;
    int chan = live.first;
    double livetime = live.second;
    int detID = detChanToDetIDMap[chan];
    double activeMass = actM4Det_g[detID]/1000;
    channelExposure[chan] = activeMass * livetime;

    if (chan%2==1 || detID==-1) continue; // don't double count detectors or include pulser monitors
    if (CheckModule(detID)==1 && detIsEnr[detID]==1) {
      m1EnrExp += channelExposure[chan];
      m1EnrActMass += activeMass;
    }
    if (CheckModule(detID)==1 && detIsEnr[detID]==0) {
      m1NatExp += channelExposure[chan];
      m1NatActMass += activeMass;
    }
    if (CheckModule(detID)==2 && detIsEnr[detID]==1) {
      m2EnrExp += channelExposure[chan];
      m2EnrActMass += activeMass;
    }
    if (CheckModule(detID)==2 && detIsEnr[detID]==0) {
      m2NatExp += channelExposure[chan];
      m2NatActMass += activeMass;
    }
    // cout << Form("Mass - detID %i  mod %i  enr? %i  mass %.3f\n", detID,CheckModule(detID),detIsEnr[detID],activeMass);
  }

  // Print results by module.
  time_t t = time(0);   // get time now
  struct tm * now = localtime( & t );
  cout << "\nCalculator results, " << now->tm_year+1900 << "/" << now->tm_mon+1 << "/" << now->tm_mday << "\n"
       << "\tRaw Veto Livetime " << vetoLive << "\n";
  if (mod1) {
    cout << "Module 1:\n"
         << "\tRaw Livetime : " << rawLive << "\n"
         << "\tVeto Deadtime : " << vetoDead << " (" << vetoDead/rawLive << ")\n"
         << "\tLN Deadtime : " << m1LNDead << " (" << m1LNDead/rawLive << ")\n"
        //  << "\tFinal Livetime : " << rawLive-m1LNDead-vetoDead << "\n"
         << "\tActive Enr Mass : " << m1EnrActMass  << "\n"
         << "\tActive Nat Mass : " << m1NatActMass << "\n"
         << "\tFinal Enr Exposure : " << m1EnrExp << "\n"
         << "\tFinal Nat Exposure : " << m1NatExp << "\n";
  }
  if (mod2) {
    cout << "Module 2:\n"
         << "\tRaw Livetime : " << rawLive << "\n"
         << "\tVeto Deadtime : " << vetoDead << " (" << vetoDead/rawLive << ")\n"
         << "\tLN Deadtime : " << m2LNDead << " (" << m2LNDead/rawLive << ")\n"
        //  << "\tFinal Livetime : " << rawLive-m2LNDead-vetoDead << "\n"
         << "\tActive Enr Mass : " << m2EnrActMass  << "\n"
         << "\tActive Nat Mass : " << m2NatActMass << "\n"
         << "\tFinal Enr Exposure : " << m2EnrExp << "\n"
         << "\tFinal Nat Exposure : " << m2NatExp << "\n";
  }

  // Subtract veto time and print channel by channel summary
  cout << "Channel summary : \n";
  for(auto &raw : channelRuntime) { // Loop over raw, not reduced livetimes for now.
    int chan = raw.first;
    int detID = detChanToDetIDMap[chan];
    if (detID==-1) continue; // don't print pulser monitor chans
    double activeMass = actM4Det_g[detID]/1000;
    cout << Form("%i  %-8i  %.2f kg  LT Raw: %.4f  LT Red: %.4f  Exp (kg-d): %.4f\n", chan, detID, activeMass, raw.second, channelLivetime[chan], channelExposure[chan]);
    // cout << Form("%i  %-7i  %.2fkg  Livetime: %.4f  Exp (kg-d): %.4f\n", chan, detID, activeMass, raw.second, channelExposure[chan]);
  }
}


int mergeIntervals(vector<pair<int,int>> vals, int start, int stop)
{
  // Start with a vector of start/stop intervals, and sort in order of increasing start times.
  sort(vals.begin(), vals.end(), compareInterval);

  // Start a `final` list of intervals.  Then loop over the inputs.
  // Compare each interval to the most recent one on the stack.
  // If the current interval doesn't overlap with the top one, push it to the stack.
  // If the current interval DOES overlap w/ the top one, update the end time of the top one.
  vector<pair<int,int>> merged = {vals[0]};
  for (size_t i = 1 ; i < vals.size(); i++)
  {
    pair<int,int> top = merged.back();

    if (top.second < vals[i].first)
      merged.push_back(vals[i]);

    else if (top.second < vals[i].second) {
      top.second = vals[i].second;
      merged.pop_back();
      merged.push_back(top);
    }
  }

  // Now that we're merged, apply a global cutoff (run boundary).
  vector<pair<int,int>>::iterator it = merged.begin();
  while (it != merged.end()){
    pair<int,int> ival = *it;
    if (ival.first < start)  { ival.first = start; *it=ival; }
    if (ival.second > stop)  { ival.second = stop; *it=ival;}
    if (ival.second < start || ival.first > stop) { merged.erase(it); } // erase increments the iterator
    else it++;
  }

  // cout << "Result.  Start " << start << " ";
  // for (auto ival : merged) cout << "[" << ival.first << "," << ival.second << "] ";
  // cout << stop << " stop" << endl;

  // Finally, sum the intervals to get the total time.
  int deadTime = 0;
  for (auto ival : merged) deadTime += ival.second - ival.first;
  return deadTime;
}


// Used to pass multiple options to the DB as a single string.
// http://stackoverflow.com/questions/236129/split-a-string-in-c
template<typename Out>
void split(const string &s, char delim, Out result) {
    stringstream ss;
    ss.str(s);
    string item;
    while (getline(ss, item, delim)) *(result++) = item;
}
vector<string> split(const string &s, char delim) {
    vector<string> elems;
    split(s, delim, back_inserter(elems));
    return elems;
}
void getDBRunList(int &dsNum, double &ElapsedTime, string options, vector<int> &runList, vector<pair<int,double>> &times)
{
  bool docs=true;

  // Parse the option string
  vector<string> opts = split(options,' '); // note the single quote
  string view = opts[0];
  string fullView = "";
  if (view == "runrank")
    fullView = Form("run_rank?key=[\"%s\",\"%s\"]",opts[1].c_str(),opts[2].c_str());
  else if (view == "dataset")
    fullView = Form("dataset?key=\"%i\"",stoi(opts[1]));

  cout << "view: " << view << endl
       << "fullview: " << fullView << endl;

  // Access DB
  const string dbString = "mjd_run_database";
  const string dbServer = "mjdb.phy.ornl.gov";
  MJDatabase runDB(&dbString, &dbServer);
  runDB.SetServerScheme("https");
  MJDocument runDoc;
  runDoc.Get_View(runDB,"dbApp",fullView,docs);
  string errorMessage;
  if (runDB.GetDBStatus(errorMessage)!=0){
    cout << "Failed to get document.  cURL error: " << runDB.GetDBStatus(errorMessage)
         << " Message: " << errorMessage << endl;
    return;
  }
  int nDocs = runDoc["rows"].Length();
  cout << "Found " << nDocs << " run records.\n";
  cout << runDB.GetURL() << endl; // you can check this in a browser
  // runDoc.Dump();

  // Loop over the document
  for (int i = 0; i < nDocs; i++)
  {
    int run = atoi(runDoc["rows"][i]["value"].Value().AsString().c_str());
    runList.push_back(run);

    // runDoc["rows"][i].Dump(); // dump just one document
    // int runInDoc = atoi(runDoc["rows"][i]["doc"]["RunNumber"].Value().AsString().c_str());
    // bool isOpen = ( runDoc["rows"][i]["doc"]["access"].Value().AsString() == "open" );
    // int runBits = atoi( runDoc["rows"][i]["doc"]["orca_run_bits"].Value().AsString().c_str() );
    // int runQuality = atoi( runDoc["rows"][i]["doc"]["RunQualityVal"].Value().AsString().c_str() );
    // string runRank = runDoc["rows"][i]["doc"]["RunRank"].Value().AsString();  // gold, silver, etc
    // string timestamp = runDoc["rows"][i]["doc"]["timestamp"].Value().AsString();
    int stopTime = atoi( runDoc["rows"][i]["doc"]["time"].Value().AsString().c_str() );
    double runElapsedTime = stod( runDoc["rows"][i]["doc"]["ElapsedTime"].Value().AsString() );
    // elapsedTimes.push_back(runElapsedTime);
    ElapsedTime += runElapsedTime;

    pair<int,double> tmp = make_pair(stopTime,runElapsedTime);
    times.push_back(tmp);

  }

  // Figure out the dataset
  dsNum = FindDataSet(runList[0]);
}


// UNTESTED:  Secondary functions to sort and merge overlapping time intervals.
// TODO: Compare this against mergeIntervals when we have
// very large lists of intervals from various dead time sources.
template <typename T>
inline bool TimeSort(const pair<T,T>& firstPair,
      const pair<T,T>& secondPair){
  return (firstPair.first < secondPair.first);
}
inline void TimeSortMerge(vector<pair<int,int> >& pulseTimes)
{
  sort(pulseTimes.begin(), pulseTimes.end(), TimeSort<int>);

  if (pulseTimes.size() == 0) return;

  vector<pair<int,int> > merged;
  pair<int, int> current = pulseTimes[0];

  for (unsigned int i=1; i < pulseTimes.size(); i++) {
    // offset -1 to fuse adjacent integers
    if (pulseTimes[i].first - 1 <= current.second) {
      if (current.second < pulseTimes[i].second)
      current.second = pulseTimes[i].second;
      } else {
      merged.push_back(current);
      current = pulseTimes[i];
    }
  }
  merged.push_back(current);

  // Exchange storage
  pulseTimes.swap(merged);
}
inline void TimeSortMerge(vector<pair<double,double> >& pulseTimes)
{
  sort(pulseTimes.begin(), pulseTimes.end(), TimeSort<double>);
  if (pulseTimes.size() == 0) return;

  vector<pair<double,double> > merged;
  pair<double, double> current = pulseTimes[0];

  for (unsigned int i=1; i < pulseTimes.size(); i++) {
    if (pulseTimes[i].first <= current.second) {
      if (current.second < pulseTimes[i].second)
      current.second = pulseTimes[i].second;
      } else {
      merged.push_back(current);
      current = pulseTimes[i];
    }
  }
  merged.push_back(current);

  // Exchange storage
  pulseTimes.swap(merged);
}


// Looks up which sub-range a particular run is in, given a range map.
void locateRunRange(int run, map<int,vector<int>> ranges, int& runInSet, int& firstRunInSet)
{
  bool foundRun = false;
  for (auto& r : ranges) {
    vector<int> thisRange = r.second;
    for (size_t i = 0; i < thisRange.size(); i+=2) {
      if (run >= thisRange[i] && run <= thisRange[i+1]) {
        // cout << Form("Found run %i between runs %i and %i\n",run,thisRange[i],thisRange[i+1]);
        foundRun = true;
        runInSet = r.first;
        firstRunInSet = thisRange[0];
        break;
      }
    }
    if (foundRun) break;
  }
  if (!foundRun) {
    cout << "HMMM, couldn't find run " << run << endl;
    runInSet = -1;
    firstRunInSet = -1;
  }
}

// Parses the 'lis' files in ./deadtime/ to make a range map
map<int, vector<int>> getDeadtimeRanges(int dsNum)
{
  map<int, vector<int>> ranges;

  // Find runlist files for this dataset
  string command = Form("ls ./deadtime/ds%i_*.lis",dsNum);
  if (dsNum==5) command = Form("ls ./deadtime/DS*.lis");
  cout << "command is " << command << endl;
  array<char, 128> buffer;
  vector<string> files;
  string str;
  shared_ptr<FILE> pipe(popen(command.c_str(), "r"), pclose);
  if (!pipe) throw runtime_error("popen() failed!");
  while (!feof(pipe.get())) {
    if (fgets(buffer.data(), 128, pipe.get()) != NULL) {
      str = buffer.data();
      str.erase(remove(str.begin(), str.end(), '\n'), str.end());  // strip newline
      files.push_back(str);
    }
  }

  // Build the ranges.  Quit at the first sign of trouble.
  int rangeCount = 0;
  for (auto file : files)
  {
    // cout << file << endl;
    ifstream lisFile(file.c_str());
    if (!lisFile) {
      cout << "Couldn't find file: " << file << endl;
      return ranges;
    }
    string buffer;
    int firstRun = -1, lastRun = -1;
    while (getline(lisFile, buffer))
    {
      size_t found = buffer.find("Run");
      if (found == string::npos) {
        cout << "Couldn't find a run expression in " << buffer << endl;
        return ranges;
      }
      int run = stoi( buffer.substr(found+3) );
      if (firstRun == -1) firstRun = run;
      lastRun = run;
    }
    ranges[rangeCount] = {firstRun,lastRun};
    rangeCount++;
  }
  return ranges;
}
