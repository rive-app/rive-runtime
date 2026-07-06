#requires pip install python-opencv-headless

import argparse
import cv2 as cv
import numpy as np
import os
import sys

parser = argparse.ArgumentParser(description=
"""
Compares candidate images against goldens by pixels, writing one status line
per image and (optionally) two diff images per image.

A single process diffs multiple images in order to amortize the cost of spawning
the Python interpreter and importing OpenCV (cv2).

Two ways to feed images to a process:
  --names    diff a fixed list of file names, then exit.
  --serve    stay alive reading one file name per line from stdin, writing
             "done" to stdout after each, so the parent (diff.py) can pull work
             dynamically and keep every worker busy until the queue is empty.

output file {status}            one status line per image
output file {name}.diff0.png    the exact abs(A - B) of each pixel
output file {name}.diff1.png    a mask which has 255 for difference, 0 otherwise
""")

mode = parser.add_mutually_exclusive_group(required=True)
mode.add_argument("-N", "--names", type=str, nargs='+', help="space-separated image file names (e.g. foo.png bar.png) to diff; each is looked up by name in the --candidate and --golden directories")
mode.add_argument("--serve", action='store_true', help="read image file names from stdin, one per line, diffing each and writing 'done' to stdout after each; exit on EOF")
parser.add_argument("-c", "--candidate", type=str, required=True, help="candidate DIRECTORY of images to diff")
parser.add_argument("-g", "--golden", type=str, required=True, help="golden DIRECTORY to compare against")
parser.add_argument("-s", "--status", type=str, required=True, help="output stats file name and location")
parser.add_argument("-o", "--outdir", type=str, help="output directory to store image diffs, if not provided then no output image is saved")
parser.add_argument("-v", "--verbose", action='store_true', help="enable verbose logging")
parser.add_argument("-l", "--log", action='store_true', help="redirect all verbose logging to a per-image file")
parser.add_argument("-H", "--histogram", action='store_true', help="compare images using histograms as an additional method for ruling out 'same' images. This should prevent subtle differences in the image that should not be visible from preventing a passing result")

args = parser.parse_args()

def remove_suffix(name, oldsuffix):
    if name.endswith(oldsuffix):
        name = name[:-len(oldsuffix)]
    return name

def verbose_log(name, val):
    if not args.verbose:
        return
    if args.log:
        with open(f"tmp/{name}.log", "a") as log_file:
            log_file.write(val+"\n")
    else:
        print(val, file=sys.stderr)

def diff_one(filename, status):
    name = remove_suffix(filename, ".png")
    candidate_path = os.path.join(args.candidate, filename)
    golden_path = os.path.join(args.golden, filename)

    candidate = None
    golden = None
    diff = None
    rgb_diff = None
    mask = None
    total_diff_count = 0
    max_diff = 0
    avg = 0.0
    hist_result = 1.0
    size_match = False
    failed = False
    try:
        verbose_log(name, f"loading {candidate_path}")
        candidate = cv.imread(candidate_path)
        verbose_log(name, f"loading {golden_path}")
        golden = cv.imread(golden_path)

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

            if args.histogram:
                # convert to HSV
                hsv_candidate = cv.cvtColor(candidate, cv.COLOR_BGR2HSV)
                hsv_golden = cv.cvtColor(golden, cv.COLOR_BGR2HSV)

                # down sample to 32x32 for the histogram
                h_bins = 32
                s_bins = 32
                histSize = [h_bins, s_bins]

                # available range for hue and saturation
                h_ranges = [0, 180]
                s_ranges = [0, 256]
                ranges = h_ranges + s_ranges

                # get our histograms
                hist_candidate = cv.calcHist(hsv_candidate, [0,1], None, histSize, ranges, accumulate=False)
                hist_golden = cv.calcHist(hsv_golden, [0,1], None, histSize, ranges, accumulate=False)

                # compare using CORREL histogram algorithm. the different options are detailed here
                # https://docs.opencv.org/3.4/d6/dc7/group__imgproc__hist.html#ga994f53817d621e2e4228fc646342d386
                # a hist_result of 1.0 means identical with this method. any variance results in a number less then 1.0
                hist_result = cv.compareHist(hist_candidate, hist_golden, cv.HISTCMP_CORREL)

    except Exception as E:
        print(f"Failed to load and process images {E}", file=sys.stderr)
        failed = True

    status.write(name + "\t")
    if failed:
        status.write("failed\n")
        verbose_log(name, "failed to load golden or candidate")
        return
    if candidate is None:
        status.write("missing_candidate\n")
        verbose_log(name, "missing candidate for " + name)
        return
    if golden is None:
        status.write("missing_golden\n")
        verbose_log(name, "missing golden for " + name)
        return
    if failed:
        status.write("failed\n")
        verbose_log(name, "failed to load golden or candidate")
        return
    if total_diff_count == 0:
        status.write("identical\n")
        verbose_log(name, "files are identical")
        return
    if not size_match:
        status.write("sizemismatch\n")
        verbose_log(name, "files are not the same size")
        return
    status.write(str(max_diff)+"\t")
    # prevent python from writing out in scientific notation
    status.write(f"{float(avg):.5f}\t")
    status.write(str(total_diff_count)+"\t")
    status.write(str(diff.shape[0]*diff.shape[1]))
    # add our histogram result as the last value of the status file or write a new line to say we are finished with this status
    if args.histogram:
        status.write(f"\t{float(hist_result):.5f}\n")
    else:
        status.write("\n")

    # save the output files if location provided
    if args.outdir:
        # color diff file location
        c_diff_path = os.path.join(args.outdir, name + ".diff0.png")
        # mask diff file location
        c_mask_path = os.path.join(args.outdir, name + ".diff1.png")
        verbose_log(name, f"writing images {c_diff_path} and {c_mask_path}")
        cv.imwrite(c_diff_path, rgb_diff)
        cv.imwrite(c_mask_path, mask)
        verbose_log(name, "finished writing output images")

def prepare_dirs():
    if args.log:
        os.makedirs("tmp", exist_ok=True)

    # make path to stats file if necessary
    if os.path.dirname(args.status):
        os.makedirs(os.path.dirname(args.status), exist_ok=True)
    if args.outdir:
        os.makedirs(args.outdir, exist_ok=True)

def main():
    # A single process handles the whole list; open the status file once and
    # append one line per image.
    prepare_dirs()
    with open(args.status, "a") as status:
        for filename in args.names:
            diff_one(filename, status)

def serve():
    # Interactive worker: the parent (diff.py) keeps this process alive and
    # feeds it one image file name per line on stdin, so cv2 is imported once
    # and then reused across many images. After each image we append its status
    # line to the status file and write "done" to stdout so the parent knows
    # we're ready for the next name. Exit on EOF.
    prepare_dirs()
    with open(args.status, "a") as status:
        for line in sys.stdin:
            filename = line.strip()
            if not filename:
                continue
            diff_one(filename, status)
            status.flush()
            sys.stdout.write("done\n")
            sys.stdout.flush()

if __name__ == '__main__':
    if args.serve:
        serve()
    else:
        main()
