#!/usr/local/bin/python

def main():

    lines = []
    with open('./deadtime/ds3_0.DT') as f:
        lines = f.readlines()[2:]

    # 0 1   2     3       4       5     6    7      8        9     10       11      12 13 14 15 16 17
    #ID pos FWHM thr_pos thr_neg dead% FWHM thr_pos thr_neg dead% dead_OR% Detector p1 p2 p3 p4 e0 e1
    nRow = 23
    deadTab = [[0] for i in range(nRow)]

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

    print "BEFORE:"
    printTab(deadTab)

    # recalculate columns 5, 9, and 10
    for idx in range(len(deadTab)):
        deadTab[idx][5] = 100 * (1 - float(deadTab[idx][12])/deadTab[idx][15])
        deadTab[idx][9] = 100 * (1 - float(deadTab[idx][13])/deadTab[idx][15])
        deadTab[idx][10] = 100 * (1 - float(deadTab[idx][14])/deadTab[idx][15])

    print "AFTER:"
    printTab(deadTab)


def printTab(deadTab):
    for d in deadTab:
        #  0 1   2     3       4       5     6    7      8        9     10       11      12 13 14 15 16 17
        #ID pos FWHM thr_pos thr_neg dead% FWHM thr_pos thr_neg dead% dead_OR% Detector p1 p2 p3 p4 e0 e1
        myStr = "%d  %d  %.2f  %.2f  %.2f  %.2f  %.2f  %.2f  %.2f  %.2f  %.2f  %s  %d  %d  %d  %d  %d  %d" % (d[0],d[1],d[2],d[3],d[4],d[5],d[6],d[7],d[8],d[9],d[10],d[11],d[12],d[13],d[14],d[15],d[16],d[17])
        print myStr





if __name__ == "__main__":
	main()
