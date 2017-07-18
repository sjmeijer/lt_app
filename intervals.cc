// Chopped & screwed from here:
// http://www.geeksforgeeks.org/merging-intervals/
// C. Wiseman

#include <iostream>
#include <algorithm>
#include <vector>
#include <sstream>
#include <iterator>

using namespace std;

bool compareInterval(pair<int,int> i1, pair<int,int> i2) { return (i1.first < i2.first); }
double sortIntervals();
void stringSplitter();

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


int main()
{
  // sortIntervals();
  stringSplitter();
}


double sortIntervals()
{
  // Start with a vector of (sorted) intervals, then sort in order of increasing start times.
  vector<pair<int,int>> vals = { {-5,-4}, {2,6}, {15,18}, {-1,3}, {8,10}, {17,19} };
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
  int start = 5, stop=20;
  vector<pair<int,int>>::iterator it = merged.begin();
  while (it != merged.end()){
    pair<int,int> ival = *it;
    if (ival.first < start)  { ival.first = start; *it=ival; }
    if (ival.second > stop)  { ival.second = stop; *it=ival;}
    if (ival.second < start || ival.first > stop) { merged.erase(it); } // erase increments the iterator
    else it++;
  }

  cout << "Result: ";
  for (auto ival : merged) cout << "[" << ival.first << "," << ival.second << "] ";
  cout << endl;

  // Finally, sum the intervals to get the total time.
  int deadTime = 0;
  for (auto ival : merged) deadTime += ival.second - ival.first;
  cout << "Total dead time: " << deadTime << endl;

  return 0;
}

void stringSplitter()
{
  string toSplit = "I should go home to Shannon";
  vector<string> x = split(toSplit,' ');
  for (auto str : x) cout << str << endl;

}