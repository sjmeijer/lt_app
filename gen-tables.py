#!/usr/bin/env python

if __name__=="__main__":

    lines = []
    with open("./lt_summary.txt") as f:
        tmp = f.readlines()[:]

        for line in tmp:
            if len(line)!=0:
                lines.append(line)

    for line in lines:
        print line