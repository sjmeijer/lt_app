#!/usr/bin/env python
import glob

def main():

    matchCt, noMatchCt = 0,0

    for dtFile in glob.glob("./deadtime/*.DT"):
    # for dtFile in glob.glob("./deadtime/DS2*.DT"):
    # for dtFile in ["./deadtime/ds3_0.DT"]:

        print dtFile
        deadTab = getDeadTab(dtFile)

        hgDeadAvg, lgDeadAvg, orDeadAvg = 0., 0., 0.
        hgCount, lgCount, orCount = 0., 0., 0.

        for det in range(len(deadTab)):

            detID = deadTab[det][0]
            hgDead = deadTab[det][5]
            lgDead = deadTab[det][9]
            orDead = deadTab[det][10]
            p1 = float(deadTab[det][12])
            p2 = float(deadTab[det][13])
            p3 = float(deadTab[det][14])
            p4 = float(deadTab[det][15])

            # calculate average dt's
            if hgDead > 0:
                hgDeadAvg += hgDead
                hgCount += 1
            if lgDead > 0:
                lgDeadAvg += lgDead
                lgCount += 1
            if orDead > 0:
                orDeadAvg += orDead
                orCount += 1

        hgDeadAvg /= hgCount
        lgDeadAvg /= lgCount
        orDeadAvg /= orCount

        print "hgDeadAvg %.2f  lgDeadAvg %.2f  orDeadAvg %.2f" % (hgDeadAvg, lgDeadAvg, orDeadAvg)

        # duplicate the ds_livetime algorithm for calculating orDead
        for det in range(len(deadTab)):

            detID = deadTab[det][0]
            hgDead = deadTab[det][5] # remember, these values are in PERCENT (ds_livetime divides by 100)
            lgDead = deadTab[det][9]
            orDead = deadTab[det][10]
            p1 = float(deadTab[det][12])
            p2 = float(deadTab[det][13])
            p3 = float(deadTab[det][14])
            p4 = float(deadTab[det][15])







        #
        # // calculate hardware deadtime and handle bad values (val<0)
        # double hgDead = dtMap[pos][0]/100.0; // value is in percent, divide by 100
        # double lgDead = dtMap[pos][1]/100.0;
        # if (hgDead < 0 && lgDead >= 0) hgDead = lgDead;
        # if (lgDead < 0 && hgDead >= 0) lgDead = hgDead;
        # if (lgDead < 0 && hgDead < 0) { hgDead = hgDeadAvg; lgDead = lgDeadAvg; }
        #
        # // calculate the "or" deadtime, correctly handling edge cases w/ no pulser counts
        # double p1=dtMap[pos][3], p2=dtMap[pos][4], p3=dtMap[pos][5], p4=dtMap[pos][6];
        # double orDead = 0;
        # if (p3 > 0 && p4 > 0) orDead = (1 - p3/p4); // normal case
        # else {
        # bool hgGood=false, lgGood=false, hgGuess=false, lgGuess=false, hgBad=false, lgBad=false;
        # if (p1 > 0 && p4 > 0) hgGood=true;
        # if (p2 > 0 && p4 > 0) lgGood=true;
        # if (p1 == 0 && hgDead >= 0.) hgGuess=true;
        # if (p2 == 0 && lgDead >= 0.) lgGuess=true;
        # if (p1 == 0 && hgDead < 0) hgBad=true;
        # if (p2 == 0 && lgDead < 0) lgBad=true;
        #
        # if (hgGood) orDead = hgDead;
        # else if (hgGuess && lgGood) orDead = lgDead;
        # else if (hgGuess && lgGuess) orDead = hgDead;
        # else if (hgGuess && lgBad) orDead = hgDead;
        # else if (hgBad && (lgGood || lgGuess)) orDead = lgDead;
        # else orDead = orDeadAvg; // punt.  will be 0 if there are somehow no good entries in the whole subset.
        # }



def getDeadTab(fileName):
    """ create a 2d list 'deadTab' to hold the deadtime table """

    lines = []
    with open(fileName) as f:
        lines = f.readlines()[2:]
    nDets = len(lines)
    deadTab = [[0] for i in range(nDets)]
    for idx in range(len(lines)):
        spl = lines[idx].split()
        tmp = []
        for st in spl:
            val = "" # handle floats, ints, and strings
            try:
                val = int(st)
            except ValueError:
                try:
                    val = float(st)
                except ValueError:
                    val = st
                    pass
            tmp.append(val)
        deadTab[idx] = tmp
    return deadTab


def printTab(deadTab):
    for d in deadTab:
        #  0 1   2     3       4       5     6    7      8        9     10       11      12 13 14 15 16 17
        #ID pos FWHM thr_pos thr_neg dead% FWHM thr_pos thr_neg dead% dead_OR% Detector p1 p2 p3 p4 e0 e1
        myStr = "%d  %d  %.2f  %.2f  %.2f  %.2f  %.2f  %.2f  %.2f  %.2f  %.2f  %s  %d  %d  %d  %d  %d  %d" % (d[0],d[1],d[2],d[3],d[4],d[5],d[6],d[7],d[8],d[9],d[10],d[11],d[12],d[13],d[14],d[15],d[16],d[17])
        print myStr


if __name__ == "__main__":
	main()
