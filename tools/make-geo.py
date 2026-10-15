import csv, gzip, ipaddress, struct, sys

def build(rows):
    out = []
    for start, end, cc in rows:
        if out and out[-1][1] == cc:
            continue
        if out and out[-1][0] == start:
            out[-1] = (start, cc)
            continue
        out.append((start, cc))
    return out

def main(src, dst):
    v4, v6 = [], []
    with gzip.open(src, "rt", newline="") as f:
        for a, b, cc in csv.reader(f):
            s, e = ipaddress.ip_address(a), ipaddress.ip_address(b)
            cc = cc.strip().upper()[:2] or "ZZ"
            if s.version == 4:
                v4.append((int(s), int(e), cc))
            else:
                v6.append((int(s) >> 64, int(e) >> 64, cc))
    v4.sort(); v6.sort()
    r4, r6 = build(v4), build(v6)
    with open(dst, "wb") as f:
        f.write(b"GEO1")
        f.write(struct.pack(">I", len(r4)))
        for s, cc in r4:
            f.write(struct.pack(">I", s) + cc.encode("ascii"))
        f.write(struct.pack(">I", len(r6)))
        for s, cc in r6:
            f.write(struct.pack(">Q", s) + cc.encode("ascii"))
    print(len(r4), len(r6))

main(sys.argv[1], sys.argv[2])
