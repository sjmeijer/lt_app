#!/usr/local/bin/python
import numpy as np
import matplotlib
import matplotlib.pyplot as plt

def main():

    # load data, make sure run lists are the same
    list1 = np.loadtxt("ds5-gatDS.txt")
    gdsRuns = list1[:,0]
    gdsTimes = list1[:,1]

    list2 = np.loadtxt("ds5-runDB.txt")
    rdbRuns = list2[:,0]
    rdbTimes = list2[:,1]
    print "Equal run lists?",np.array_equal(gdsRuns,rdbRuns)

    # find time diff's, output a list of particularly bad ones
    timeDiffs = gdsTimes - rdbTimes
    idx = np.where(timeDiffs > 10)
    badRuns = gdsRuns[idx]
    badTimes = timeDiffs[idx]
    # for i in range(len(badRuns)):
        # print "%i  %.1f" % (badRuns[i],badTimes[i])
    # print badRuns

    # print the total diff
    gdsTotal, rdbTotal = np.sum(gdsTimes)/86400, np.sum(rdbTimes)/86400
    print "gds total %.3f  rdb total %.3f  diff %.3f" % (gdsTotal,rdbTotal,gdsTotal-rdbTotal)

    # make a plot of the diff's
    matplotlib.rcParams.update({'font.size': 18})
    fig = plt.figure(figsize=(8,5), facecolor='w')
    p1 = plt.subplot(111)
    p1.set_ylabel("Difference (seconds)",y=0.95, ha='right')
    p1.set_xlabel("Run Number",x=0.95, ha='right')
    p1.set_title("DS5: GATDataSet Time - ElapsedTime")
    p1.plot(gdsRuns,timeDiffs,'o',markersize=3,color='blue')
    plt.grid()
    plt.tight_layout()
    plt.show(block=False)
    plt.savefig("./livetime-diffs.pdf")


if __name__ == "__main__":
	main()
