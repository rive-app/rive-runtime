#requires pip install python-opencv-headless

import argparse
import cv2 as cv
import numpy as np
import os

parser = argparse.ArgumentParser(description=
"""
Compares to images by pixels, outputing two different images for the differences. 

output file {status}
output file {name}.diff0.png
    The exact abs(A - B) of each pixel
output file {name}.diff1.png
    A mask which has 1 for difference and 0 for no difference at a given pixel
""")

parser.add_argument("-n", "--name", type=str, required=True, help="name used for output images")
parser.add_argument("-c", "--candidate", type=str, required=True, help="candidate for image diffing")
parser.add_argument("-g", "--golden", type=str, required=True, help="golden to compare against")
parser.add_argument("-s", "--status", type=str, required=True, help="output stats file name and location")
parser.add_argument("-o", "--outdir", type=str, help="output directory to store image diffs, if not provided then no output image is saved")
parser.add_argument("-v", "--verbose", action='store_true', help="enable verbose logging")
parser.add_argument("-l", "--log", action='store_true', help="redirect all verbose logging to a file with --name")

args = parser.parse_args()

def verbose_log(val):
    if not args.verbose:
        return
    if args.log:
        with open(f"tmp/{args.name}.log", "a") as log_file:
            log_file.write(val+"\n")
    else:
        print(val)

def main():
    if args.log:
        os.makedirs("tmp", exist_ok=True)

    size_match = False
    failed = False
    try:
        verbose_log(f"loading {args.candidate}")
        candidate = cv.imread(args.candidate)
        verbose_log(f"loading {args.golden}")
        golden = cv.imread(args.golden)

        if candidate is not None and golden is not None:
            size_match = candidate.shape == golden.shape
            # get absolute difference between golden and candidate
            diff = cv.absdiff(golden, candidate)
            #convert diff to grey scale to make calculations easier
            grey_diff = cv.cvtColor(diff, cv.COLOR_BGR2GRAY)
            #convert to rgb for output png
            rgb_diff = cv.cvtColor(diff, cv.COLOR_BGR2RGB)
            # convert the diff to just white where a difference is
            _, mask = cv.threshold(grey_diff, 0, 255, cv.THRESH_BINARY)
            # get the total number of different pixels
            total_diff_count = cv.countNonZero(grey_diff)
            # get the max channel independent difference
            max_diff = int(diff.max())
            # get average pixel diff, this result is slitely different then imageDiff but its very close.
            # the only difference is that we first convert the diff to grey scale rather than adding the max of every channel
            avg = grey_diff.sum() / (grey_diff.shape[0]*grey_diff.shape[1]*255)
    except Exception as E:
        print(f"Failed to load and process images {E}")
        failed = True

    # make path to stats file if neccecary
    if os.path.dirname(args.status):
        verbose_log(f"making status file path {os.path.dirname(args.status)}")
        os.makedirs(os.path.dirname(args.status), exist_ok=True)
    verbose_log(f"making status file {args.status}")
    with open(args.status, "a") as status:
        status.write(args.name + "\t");
        if (candidate is None or golden is None) or failed:
            status.write("failed\n")
            verbose_log("failed to load golden or candidate")
            return
        if total_diff_count == 0:
            status.write("identical\n")
            verbose_log("files are identitcal")
            return
        if not size_match:
            status.write("sizemismatch\n")
            verbose_log("files are not the same size")
            return
        status.write(str(max_diff)+"\t")
        # prevent python from wirting out in scientific notation
        status.write(f"{float(avg):.5f}\t")
        status.write(str(total_diff_count)+"\t")
        status.write(str(diff.shape[0]*diff.shape[1])+"\n")
    verbose_log("status file finished")
    # save the output file if location provided
    if args.outdir:
        # color diff file location
        c_diff_path = os.path.join(args.outdir, args.name + ".diff0.png")
        # mask diff file location 
        c_mask_path = os.path.join(args.outdir, args.name + ".diff1.png")
        # create output dir if needed
        verbose_log(f"making output directory {args.outdir}")
        os.makedirs(args.outdir, exist_ok=True)
        verbose_log(f"writing images {c_diff_path} and {c_mask_path}")
        cv.imwrite(c_diff_path, rgb_diff)
        cv.imwrite(c_mask_path, mask)
        verbose_log("finished writing output images")

if __name__ == '__main__':
    main()
