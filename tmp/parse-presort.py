#!/usr/bin/env python3

dcrDetLabel = {
    "C1P1D1":0, "C1P1D2":1, "C1P1D3":2, "C1P1D4":3,
    "C1P2D1":4, "C1P2D2":5, "C1P2D3":6, "C1P2D4":7,
    "C1P3D1":8, "C1P3D2":9, "C1P3D3":10, "C1P3D4":11,
    "C1P4D1":12, "C1P4D2":13, "C1P4D3":14, "C1P4D4":15, "C1P4D5":16,
    "C1P5D1":17, "C1P5D2":18, "C1P5D3":19, "C1P5D4":20,
    "C1P6D1":21, "C1P6D2":22, "C1P6D3":23, "C1P6D4":24,
    "C1P7D1":25, "C1P7D2":26, "C1P7D3":27, "C1P7D4":28,
    "C2P1D1":29, "C2P1D2":30, "C2P1D3":31, "C2P1D4":32,
    "C2P2D1":33, "C2P2D2":34, "C2P2D3":35, "C2P2D4":36, "C2P2D5":37,
    "C2P3D1":38, "C2P3D2":39, "C2P3D3":40,
    "C2P4D1":41, "C2P4D2":42, "C2P4D3":43, "C2P4D4":44, "C2P4D5":45,
    "C2P5D1":46, "C2P5D2":47, "C2P5D3":48, "C2P5D4":49,
    "C2P6D1":50, "C2P6D2":51, "C2P6D3":52, "C2P6D4":53,
    "C2P7D1":54, "C2P7D2":55, "C2P7D3":56, "C2P7D4":57
    }

ins = ["DS23972.pre","DS24334.pre","DS24569.pre","DS24569.pre","DS25000.pre","DS25508.pre"]
outs = ["DS23972.DT","DS24334.DT","DS24569.DT","DS24569.DT","DS25000.DT","DS25508.DT"]

# ins = ["DS25000.pre"]
# outs = ["DS25000.DT"]

for iF, pre in enumerate(ins):

    with open(pre, "r") as f:
        table = f.readlines()

    fwhmTable = {}
    pulsTable = {}

    tableNum = 0
    for idx, line, in enumerate(table):

        tmp = (line.rstrip()).split()
        if len(tmp) == 0: continue
        if tmp[0] == 'FWHM':
            tableNum = 1
        elif tmp[0] == 'Pulser':
            tableNum = 2

        # only 'required' quantities:
        # det (CPD), hgDead, lgDead, orDead, p1, p2, p3, p4

        if tableNum == 1:
            if tmp[0] in ['FWHM','Detector']: continue

            # this table could presumably provide fwhm and HG/LG threshold values,
            # but idk how to interpret the table.  set values to 0 for now.
            hgFWHM, hgNeg, hgPos = 0., 0., 0.
            lgFWHM, lgNeg, lgPos = 0., 0., 0.
            det = tmp[1]

            fwhmTable[det] = [hgFWHM, hgNeg, hgPos, lgFWHM, lgNeg, lgPos]

        if tableNum == 2:
            if tmp[0] in ['Pulser','Detector']: continue

            id = tmp[0]
            det = tmp[2]
            p1, p2, p3, p4 = int(tmp[3]), int(tmp[4]), int(tmp[5]), int(tmp[6])
            hgDead = 100 * (1 - p1 / p4)
            lgDead = 100 * (1 - p2 / p4)
            orDead = 100 * (1 - p3 / p4)

            pulsTable[det] = [hgDead, lgDead, orDead, p1, p2, p3, p4]

    # check we have entries for everything
    if set(fwhmTable.keys()) != set(pulsTable.keys()):
        a = set(fwhmTable.keys())
        b = set(pulsTable.keys())
        print("Missing detectors:",[list(b-a), list(a-b)])

    # now fill the output file
    print("Writing",outs[iF])
    with open(outs[iF],'w') as outFile:

        header="""#                      HG                              LG\n#ID pos   FWHM thr_pos  thr_neg  dead%    FWHM thr_pos  thr_neg  dead%  dead_OR% Detector     Pulser counts\n"""
        outFile.write(header)

        for det in sorted(set(fwhmTable.keys())):

            detNum = dcrDetLabel[det]
            posWrong = "%s%s%s" % (det[1],det[3],det[3])
            hgFWHM, hgNeg, hgPos, lgFWHM, lgNeg, lgPos = fwhmTable[det]
            hgDead, lgDead, orDead, p1, p2, p3, p4 = pulsTable[det]

            # ID pos   FWHM thr_pos  thr_neg  dead%    FWHM thr_pos  thr_neg  dead%  dead_OR% Detector     Pulser counts
            # 1 111   0.61    1.38    0.72    1.63    0.27    0.62    0.40    0.09     0.05   C1P1D2     2112    2145    2146    2147 0 0

            tableEntry = "%d %s %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %s %d %d %d %d %d %d\n" % (detNum,posWrong,hgFWHM,hgNeg,hgPos,hgDead,lgFWHM,lgNeg,lgPos,lgDead,orDead,det,p1,p2,p3,p4,0,0)

            tableEntry = tableEntry.replace(' ','\t')

            outFile.write(tableEntry)
            # print(tableEntry)

