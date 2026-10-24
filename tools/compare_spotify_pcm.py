"""Compare a locally generated decoder fixture with its PCM reference."""
import argparse
from array import array
import json
import math
from pathlib import Path
import sys


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory",type=Path)
    args=parser.parse_args();samples=[]
    for name in ("decoded.pcm","reference.pcm"):
        raw=(args.directory/name).read_bytes()
        if len(raw)!=529200: raise ValueError("Fixture PCM length differs")
        pcm=array("h");pcm.frombytes(raw)
        if sys.byteorder!="little": pcm.byteswap()
        samples.append(pcm)
    actual,reference=samples
    error=sum((x-y)**2 for x,y in zip(actual,reference))
    energy=sum(y*y for y in reference)
    rmse=math.sqrt(error/len(actual));snr=10*math.log10(energy/max(1,error))
    if rmse>=3 or snr<=40: raise ValueError("Fixture PCM differs from reference")
    path=args.directory/"metrics.json";metrics=json.loads(path.read_text())
    metrics.update(reference_rmse=rmse,reference_snr_db=snr,peak_sample=max(abs(x) for x in actual))
    path.write_text(json.dumps(metrics,indent=2))
    print(f"Vorbis PCM reference: samples={len(actual)}, RMSE={rmse:.6f}, SNR={snr:.3f} dB PASS")


if __name__=="__main__": main()
