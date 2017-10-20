#!/usr/bin/env python
import glob

def main():

    matchCt, noMatchCt = 0,0

    # for dtFile in glob.glob("./deadtime/*.DT"):
    for dtFile in glob.glob("./deadtime/DS2*.DT"):

        print dtFile
        deadTab = getDeadTab(dtFile)
        for det in range(len(deadTab)):

            detID = deadTab[det][0]
            dtHG = deadTab[det][5]
            dtLG = deadTab[det][9]
            dtOR = deadTab[det][10]
            p1 = float(deadTab[det][12])
            p2 = float(deadTab[det][13])
            p3 = float(deadTab[det][14])
            p4 = float(deadTab[det][15])

            isHG, isLG = False, False
            if abs(dtHG - dtOR) < 0.01: isHG = True
            if abs(dtLG - dtOR) < 0.01: isLG = True
            if isHG or isLG:
                matchCt += 1
            if not isHG and not isLG:
                noMatchCt += 1
                # print detID, dtHG, dtLG, dtOR

            dtHGTrue, dtLGTrue = False, False
            pctHG, pctLG, pctOR = 0., 0., 0.
            if p4 != 0:
                pctHG = (1 - p1/p4) * 100 if p1!=0 else -9.99
                pctLG = (1 - p2/p4) * 100 if p2!=0 else -9.99
                pctOR = (1 - p3/p4) * 100 if p3!=0 else -9.99
            else:
                pctHG, pctLG, pctOR = -9.99, -9.99, -9.99

            if abs(dtHG - pctHG) < 0.01: dtHGTrue = True
            if abs(dtLG - pctLG) < 0.01: dtLGTrue = True

            if dtLGTrue and not dtHGTrue:
                print "HG bad", "%d  HG %.2f %.2f  LG %.2f %.2f  OR %.2f %.2f" % (detID, dtHG, pctHG, dtLG, pctLG, dtOR, pctOR)
            if dtHGTrue and not dtLGTrue:
                print "LG bad", "%d  HG %.2f %.2f  LG %.2f %.2f  OR %.2f %.2f" % (detID, dtHG, pctHG, dtLG, pctLG, dtOR, pctOR)
            if not dtHGTrue and not dtLGTrue:
                print "Both bad", "%d  HG %.2f %.2f  LG %.2f %.2f  OR %.2f %.2f" % (detID, dtHG, pctHG, dtLG, pctLG, dtOR, pctOR)

        # return



    print "matchCt %d  noMatchCt %d" % (matchCt, noMatchCt)


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
