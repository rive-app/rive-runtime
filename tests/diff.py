#!/usr/bin/python

# get opencv dependency if needed. We do it here for imageDiff 
# because we spawn multiple processes so we would have a race condition with each one trying to check and download opencv
import subprocess
import os.path
import pathlib
import sys
from collections import defaultdict

if not "NO_VENV" in os.environ.keys():
    from venv import create
    # create venv here and then install package
    VENV_DIR = os.path.realpath(os.path.join(os.path.dirname(__file__), ".pyenv"))
    if sys.platform.startswith('win32'):
        PYTHON = os.path.join(VENV_DIR, "Scripts", "python.exe")
    else:
        PYTHON = os.path.join(VENV_DIR, "bin", "python")

    if not os.path.exists(VENV_DIR):
        create(VENV_DIR, with_pip=True)
        subprocess.check_call([PYTHON, "-m", "pip", "install", "opencv-python-headless"])
else:
    PYTHON = os.path.realpath(sys.executable)

TEMPLATE_PATH = os.path.join(os.path.dirname(__file__), "template")

from genericpath import exists
import argparse
import glob
import csv
from multiprocessing import Pool
from functools import partial
from xml.etree import ElementTree as ET
from typing import TypeVar
import shutil
import json

parser = argparse.ArgumentParser(description="Compare two directories of images")
parser.add_argument("--goldens", "-g", required=True, help="INPUT directory of correct images")
parser.add_argument("--candidates", "-c", required=True, help="INPUT directory of candidate images")
parser.add_argument("--output", "-o", required=True, help="OUTPUT directory to store differences")
parser.add_argument("--verbose", "-v", action='store_true', help="enable verbose output")
parser.add_argument("--build", "-b", default='release', choices=['debug', 'release'], help="build configuration")
parser.add_argument("-j", "--jobs", default=1, type=int, help="number of jobs to run in parallel")
parser.add_argument("-r", "--recursive", action='store_true', help="recursively diffs images in \"--candidates\" sub folders against \"--goldens\"")
parser.add_argument("-p", "--pack", action='store_true', help="copy candidates and goldens into output folder along with results")
parser.add_argument("-H", "--histogram_compare", action='store_true', help="Use histogram compare method to determine if candidate matches gold")
parser.add_argument("-t", "--threshold", default=0.01, type=float, help="if histogram_compare is set, then threshold used for histogram pass result otherwise the threshold for pixel diff pass result")

clean_mode = parser.add_mutually_exclusive_group(required=False)
clean_mode.add_argument("-x", "--clean", action='store_true', help="delete golden and candidate images that are identical, also dont add identical images to index.html")
clean_mode.add_argument("-f", "--fails_only", action='store_true', help="delete images of all tests except for fails, also only adds failing tests to index.html, acts the same as -x if histogram_compare is false")

args = parser.parse_args()

# _winapi.WaitForMultipleObjects only supports 64 handles, which we exceed if we span >61 diff jobs.
args.jobs = min(args.jobs, 61)

status_filename_base = "_imagediff_status"
status_filename_pattern = f"{status_filename_base}_%i_*.txt" % os.getpid()
show_commands = False

class TestEntry(object):
    pass_entry_template:str = None
    error_entry_template:str = None
    identical_entry_template:str = None
    missing_file_entry_template:str = None

    @classmethod
    def load_templates(cls, path):
        with open(os.path.join(path, "error_entry.html")) as t:
            cls.error_entry_template = t.read()
        with open(os.path.join(path, "pass_entry.html")) as t:
            cls.pass_entry_template = t.read()
        with open(os.path.join(path, "identical_entry.html")) as t:
            cls.identical_entry_template = t.read()
        with open(os.path.join(path, "missing_file_entry.html")) as t:
            cls.missing_file_entry_template = t.read()

    def __init__(self, words, candidates_path, golden_path, output_path, device_name=None, browserstack_details=None):
        self.diff0_path_abs = None
        self.diff1_path_abs = None
        self.device = device_name
        self.browserstack_details = browserstack_details
        self.name = words[0]
        self.candidates_path_abs = os.path.join(candidates_path, f"{self.name}.png")
        if args.pack and device_name is not None:
            self.candidates_path = os.path.join(device_name, f"{self.name}.png")
            self.golden_path = os.path.join("golden", f"{self.name}.png")
        elif args.recursive:
            self.candidates_path = pathlib.Path(os.path.relpath(os.path.join(candidates_path, f"{self.name}.png"),  pathlib.Path(output_path).parent.absolute())).as_posix()
            self.golden_path = pathlib.Path(os.path.relpath(os.path.join(golden_path, f"{self.name}.png"), pathlib.Path(output_path).parent.absolute())).as_posix()
        else:
            self.candidates_path = pathlib.Path(os.path.relpath(os.path.join(candidates_path, f"{self.name}.png"), output_path)).as_posix()
            self.golden_path = pathlib.Path(os.path.relpath(os.path.join(golden_path, f"{self.name}.png"), output_path)).as_posix()

        if len(words) == 2:
            self.avg = None
            self.histogram = None
            self.type = words[1]
        else:    
            self.max_diff = int(words[1])
            self.avg = float(words[2])
            self.total_diff_count = int(words[3])
            self.total_pixels = int(words[4])
            self.diff0_path_abs = os.path.join(output_path, f"{self.name}.diff0.png")
            self.diff1_path_abs = os.path.join(output_path, f"{self.name}.diff1.png")
            if device_name is not None:
                self.diff0_path = os.path.join(device_name, f"{self.name}.diff0.png")
                self.diff1_path = os.path.join(device_name, f"{self.name}.diff1.png")
            else:
                self.diff0_path = os.path.relpath(os.path.join(output_path, f"{self.name}.diff0.png"), output_path)
                self.diff1_path = os.path.relpath(os.path.join(output_path, f"{self.name}.diff1.png"), output_path)
            if len(words) == 6:
                self.histogram = float(words[5])
                if self.histogram < (1.0-args.threshold):
                    self.type = "failed"
                else:
                    self.type = "pass"
            else:
                self.histogram = None
                if self.max_diff > args.threshold:
                    self.type = "failed"
                else:
                    self.type = "pass"

    # this is equivalent of implementing == we are comparing by name for when we check against the correct golds to delete
    def __eq__(self, other):
        return self.name == other.name

    # hash by name so that when we create a set out of entry list we condense it down to the number of goldens since we only care about them
    def __hash__(self):
        return hash(self.name)

    # this is equivalent of implementing < operator. We use this for sorted and sort functions
    def __lt__(self, other):
        # Always sort by avg first. Histogram is a good heuristic to divide into
        # "pass/fail" buckets, but it's helpful to then see the fail bucked
        # sorted by avg, which is more sensitive to differences.
        if (self.avg == other.avg and
            self.histogram is not None and
            other.histogram is not None):
            # LOWER histogram values mean worse matches. Sort the bad matches first.
            return self.histogram > other.histogram
        else:
            # HIGHER avg values mean worse matches. Sort the bad matches first.
            return self.avg < other.avg

    def __str__(self):
        vals = dict()
        vals['name'] = f"{self.name} ({self.device})" if self.device is not None else self.name
        vals['url'] = self.browserstack_details['browser_url'] if self.browserstack_details is not None else ' '
        if self.type == 'missing_golden':
            # show candidate, since golden is missing
            vals['image'] = self.candidates_path
            return self.missing_file_entry_template.format_map(vals)
        
        elif self.type == 'missing_candidate':
            # show golden, since candidate is missing
            vals['image'] = self.golden_path
            return self.missing_file_entry_template.format_map(vals)

        vals['golden'] = self.golden_path
        vals['candidate'] = self.candidates_path
        
        if self.type == "pass" or self.type == "failed":
            vals['max'] = self.max_diff
            vals['avg'] = self.avg
            vals['total_diff'] = self.total_diff_count
            vals['percent'] = float(self.total_diff_count) / float(self.total_pixels)
            vals['histogram'] = self.histogram if self.histogram is not None else 'None'
            vals['diff0'] = self.diff0_path
            vals['diff1'] = self.diff1_path

            if self.type == 'pass':
                return self.pass_entry_template.format_map(vals)
            else:
                return self.error_entry_template.format_map(vals)

        if self.type == 'identical':
            return self.identical_entry_template.format_map(vals)
        
        return ''

    def clean(self):
        if args.verbose:
            print(f"cleaning TestEntry {self.name} - {self.device}")
        # removes image files to save space, this is done for clean and fails only arguments
        os.remove(self.candidates_path_abs)
        # if we are packing the files, also delete the packed ones
        if args.pack:
            os.remove(os.path.join(args.output, self.candidates_path))
        # if we have diff files, delete those too
        if self.diff0_path_abs is not None:
            os.remove(self.diff0_path_abs)
        # this should always be true if diff0 is not none but just to be safe we do a separate check
        if self.diff1_path_abs is not None:
            os.remove(self.diff1_path_abs)


    @property
    def success(self):
        return self.type == "pass" or self.type == "identical"
    
    @property
    def csv_dict(self):
        val = dict()

        val['file_name'] = self.name
        val['original'] = self.golden_path
        val['candidate'] = self.candidates_path
        if self.type == "pass" or self.type == "failed":
            val['max_rgb'] = str(self.max_diff)
            val['avg_rgb'] = str(self.avg)
            val['pixel_diff_count'] = str(self.total_diff_count)
            val['pixel_diff_percent'] = '100'
            if self.histogram is not None:
                val['hist_result'] = str(self.histogram)
            val['color_diff'] = self.diff0_path
            val['pixel_diff'] = self.diff1_path
        elif self.type == 'identical':
            val['max_rgb'] = '0'
            val['avg_rgb'] = '0'
            val['pixel_diff_count'] = '0'
            val['pixel_diff_percent'] = '100'
            if args.histogram_compare:
                val['hist_result'] = '1.0'
            val['color_diff'] = ''
            val['pixel_diff'] = ''

        return val
    

def shallow_copy_images(src, dest):
    file_names = [file for file in os.scandir(src) if file.is_file() and '.png' in file.name]
    for file in file_names:
        shutil.copyfile(file.path, os.path.join(dest, file.name))

def remove_suffix(name, oldsuffix):
    if name.endswith(oldsuffix):
        name = name[:-len(oldsuffix)]
    return name

def write_csv(entries, origpath, candidatepath, diffpath, missing_candidates):
    origpath = os.path.relpath(origpath, diffpath)
    candidatepath = os.path.relpath(candidatepath, diffpath)

    height = 256

    with open(os.path.join(diffpath, "data.csv"), "w") as csvfile:
        fieldnames = ['file_name','original', 'candidate',  'max_rgb', 'avg_rgb', 'pixel_diff_count', 'pixel_diff_percent', 'color_diff', 'pixel_diff']
        if args.histogram_compare:
            fieldnames.extend(['hist_result'])
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        writer.writeheader()
        for name in missing_candidates:
            if args.histogram_compare:
                writer.writerow({
                    'file_name': name.split('.')[0],
                    'original': os.path.join(origpath, name), 
                    'candidate': '',
                    'max_rgb':255,
                    'avg_rgb':255,
                    # i guess its all of em, not 1 of em, but whatever
                    'pixel_diff_count': 1,
                    'pixel_diff_percent': '100',
                    'hist_result' : 1.0,
                    'color_diff': '',
                    'pixel_diff': ''
                })
            else:
                writer.writerow({
                    'file_name': name.split('.')[0],
                    'original': os.path.join(origpath, name), 
                    'candidate': '',
                    'max_rgb':255,
                    'avg_rgb':255,
                    # i guess its all of em, not 1 of em, but whatever
                    'pixel_diff_count': 1,
                    'pixel_diff_percent': '100',
                    'color_diff': '',
                    'pixel_diff': ''
                })

        for entry in entries:
            writer.writerow(entry.csv_dict)
            

def write_min_csv(total_passing, total_failing, total_missing_candidates, total_missing_goldens, total_identical, total_entries, csv_path):
    # delete and old data
    if os.path.exists(csv_path):
        os.remove(csv_path)

    with open(csv_path, 'w', newline='') as csv_file:
        csv_writer = csv.DictWriter(csv_file, fieldnames=['type', 'number'])
        csv_writer.writerow({'type':'failed', 'number' : str(total_failing)})
        csv_writer.writerow({'type':'pass', 'number' : str(total_passing)})
        csv_writer.writerow({'type':'missing_candidates', 'number' : str(total_missing_candidates)})
        csv_writer.writerow({'type':'missing_goldens', 'number' : str(total_missing_goldens)})
        csv_writer.writerow({'type':'identical', 'number' : str(total_identical)})
        csv_writer.writerow({'type':'total', 'number' : str(total_entries)})

def call_imagediff(filename, golden, candidate, output, parent_pid):
    cmd = [PYTHON, "image_diff.py",
           "-n", remove_suffix(filename, ".png"),
           "-g", os.path.join(golden, filename),
           "-c", os.path.join(candidate, filename),
           # Each process writes its own status file in order to avoid race conditions.
           "-s", "%s_%i_%i.txt" % (status_filename_base, parent_pid, os.getpid())]
    if output is not None:
        cmd.extend(["-o", output])
    if args.verbose:
        cmd.extend(["-v", "-l"])
    if args.histogram_compare:
        cmd.extend(["-H"])
    
    if show_commands:
        str = ""
        for c in cmd:
            str += c + " "
        print(str)
    
    if 0 != subprocess.call(cmd):
        print("Error calling " + cmd[0])
        return -1

def parse_status(candidates_path, golden_path, output_path, device_name, browserstack_details):
    total_lines = 0
    test_entries = []
    success = True
    status_files = glob.glob(status_filename_pattern)
    if not status_files:
        print('Not a single status file got written, are you just starting new?')
    for status_filename in status_files:
        for line in open(status_filename, "r").readlines():
            total_lines += 1
            words = line.rstrip().split('\t')
            entry = TestEntry(words, candidates_path, golden_path, output_path, device_name, browserstack_details)
            test_entries.append(entry)
            if not entry.success:
                success = False

    return (total_lines, test_entries, success)

def diff_directory_shallow(candidates_path, output_path, golden_path, device_name=None, browserstack_details=None):
    original_filenames = set((file.name for file in os.scandir(candidates_path)
                              if file.is_file() and file.name.endswith('.png')))
    candidate_filenames = set(os.listdir(golden_path))
    intersect_filenames = original_filenames.intersection(candidate_filenames)

    missing = []
    for file in original_filenames.difference(candidate_filenames):
        print(f'Candidate file {file} missing in goldens.')
        missing.append(TestEntry([file.split('.')[0], 'missing_golden'], candidates_path, golden_path, output_path, device_name, browserstack_details))

    for file in candidate_filenames.difference(original_filenames):
        print(f'Golden file {file} missing in candidates.')
        missing.append(TestEntry([file.split('.')[0], 'missing_candidate'], candidates_path, golden_path, output_path, device_name, browserstack_details))

    if args.jobs > 1:
        print("Diffing %i candidates in %i processes..." % (len(intersect_filenames), args.jobs))
    else:
        print("Diffing %i candidates..." % len(intersect_filenames))
    sys.stdout.flush()

#   generate the diffs (if any) and write to the status file
    f = partial(call_imagediff,
                golden=golden_path,
                candidate=candidates_path,
                output=output_path,
                parent_pid=os.getpid())
    
    Pool(args.jobs).map(f, intersect_filenames)
    (total_lines, entries, success) = parse_status(candidates_path, golden_path, output_path, device_name, browserstack_details)
    
    entries.extend(missing)

    print(f'finished with Succes:{success} and {total_lines} lines')

    if total_lines != len(intersect_filenames):
        print(f"Internal failure: Got {total_lines} status lines. Expected {len(intersect_filenames)}.")
        success = False

    if original_filenames.symmetric_difference(candidate_filenames):
        print("golden and candidate directories do not have identical files.")
        success = False

#   cleanup our scratch files
    for status_filename in glob.iglob(status_filename_pattern):
        os.remove(status_filename)

    return (entries, missing, success)

# Sort descending total failure count (missing + failure), then by descending failure count, then by name
# (put the failed ones at the top)
def device_entry_sort_key(kv): 
    return (-(kv[1]["missing_candidate"] + kv[1]["failed"]), -(kv[1]["failed"]), kv[0])

# returns entries sorted into identical, passing and failing as well as html str list of each
# based on arguments passed, we may or may not return all of the string lists, but we always return the object lists
def sort_entries(entries):
    # we dont need an intermediate object list because we never sort ot clean these, so direct to html
    missing_golden_str = [str(entry) for entry in entries if entry.type == "missing_golden"]
    missing_candidate_str = [str(entry) for entry in entries if entry.type == "missing_candidate"]

    failed_entires = [entry for entry in entries if entry.type == "failed"]
    pass_entires = [entry for entry in entries if entry.type == "pass"]
    identical_entires = [entry for entry in entries if entry.type == "identical"]

    sorted_failed_entires = sorted(failed_entires, reverse=True)

    sorted_failed_str = [str(entry) for entry in sorted_failed_entires]

    # Build a list of stat counts by device name
    all_device_stats = dict()
    for entry in entries:
        if entry.device not in all_device_stats:
            all_device_stats[entry.device] = defaultdict(int)
            all_device_stats[entry.device]["url"] = entry.browserstack_details['browser_url'] if entry.browserstack_details is not None else ' '
        all_device_stats[entry.device][entry.type] += 1

    # Now build the device summary text using the template
    device_summary_str = ""
    with open(os.path.join(TEMPLATE_PATH, "device_summary_entry.html")) as t:
        device_summary_entry_template = t.read()

        for device_name, device_summary in sorted(all_device_stats.items(), key=device_entry_sort_key):
            none_succeeded = device_summary["pass"] == 0 and device_summary["identical"] == 0
            any_failed = device_summary["failed"] > 0
            any_missing = device_summary["missing_candidate"] > 0
            device_summary_str += device_summary_entry_template.format(
                name=device_name, 
                url=device_summary["url"],
                failed_count=device_summary["failed"] if any_failed else "-",
                failed_class="fail" if any_failed else "deemphasize",
                missing_count=device_summary["missing_candidate"] if any_missing else "-",
                missing_class="fail" if any_missing else "deemphasize",
                pass_count=device_summary["pass"],
                pass_class="fail" if none_succeeded else "success",
                identical_count=device_summary["identical"],
                identical_class="fail" if none_succeeded else "" if device_summary["identical"] == 0 else "success")

    # if we are only doing fails then only sort those and return empty html lists for "pass" and "identical" we still build and return
    # identical and pass object lists for cleaning, but we dont bother sorting them
    if args.fails_only:
        return (
            sorted_failed_entires,
            pass_entires,
            identical_entires,
            sorted_failed_str,
            [],
            [],
            missing_golden_str,
            missing_candidate_str,
            device_summary_str,
            len(all_device_stats))

    
    # now sort passed entires and build the html list
    sorted_passed_entires = sorted(pass_entires, reverse=True)
    sorted_passed_str = [str(entry) for entry in sorted_passed_entires]

    # if we are cleaning then return empty html list for identical. do everything else the same
    if args.clean:
         return (sorted_failed_entires, sorted_passed_entires, identical_entires, sorted_failed_str, sorted_passed_str, [], missing_golden_str, missing_candidate_str, device_summary_str, len(all_device_stats))

    # otherwise build identical html entry list and include it in the return
    identical_str = [str(entry) for entry in identical_entires]

    return (
        sorted_failed_entires,
        sorted_passed_entires,
        identical_entires,
        sorted_failed_str,
        sorted_passed_str,
        identical_str,
        missing_golden_str,
        missing_candidate_str,
        device_summary_str,
        len(all_device_stats))

def write_html(templates_path, failed_entries, passing_entries, identical_entries, missing_golden_entries, missing_candidate_entries, device_summary_str, device_number, output_path):
    with open(os.path.join(templates_path, "index.html")) as t:
        index_template = t.read()
    
    html = index_template.format(identical=" ".join(identical_entries), passing=" ".join(passing_entries),
                                 failed=" ".join(failed_entries), failed_number=len(failed_entries),
                                 passing_number=len(passing_entries), identical_number=len(identical_entries),
                                 missing_candidate=" ".join(missing_candidate_entries), missing_candidate_number=len(missing_candidate_entries),
                                 missing_golden=" ".join(missing_golden_entries), missing_golden_number=len(missing_golden_entries),
                                 device_summaries=device_summary_str, device_number=device_number,
                                 pass_hide_class="hidden" if args.fails_only else "",
                                 identical_hide_class="hidden" if args.fails_only or args.clean else "")

    with open(os.path.join(output_path, "index.html"), "w") as file:
        file.write(html)
    
    #copy our icon to the output folder
    shutil.copyfile(os.path.join(TEMPLATE_PATH, "favicon.ico"), os.path.join(output_path, "favicon.ico"))

def diff_directory_deep(candidates_path, output_path):
    golden_path = args.goldens
    if args.pack:
        new_golden_path = os.path.join(output_path, "golden")
        os.makedirs(new_golden_path, exist_ok=True)
        shallow_copy_images(args.goldens, new_golden_path)
        golden_path = new_golden_path
    
    all_entries = []

    for folder in os.scandir(candidates_path):
        if folder.is_dir():
            if folder.name[0] == '.':
                continue
            output = os.path.join(output_path ,folder.name)
            os.makedirs(output, exist_ok=True)
            browserstack_details = None
            browserstack_details_path = os.path.join(folder.path, "session_details")
            if os.path.exists(browserstack_details_path):
                with open(browserstack_details_path, 'rt') as file:
                    browserstack_details = json.load(file)
                os.remove(browserstack_details_path)
            
            (entries, _, _) = diff_directory_shallow(folder.path, output, golden_path, folder.name, browserstack_details)
            
            all_entries.extend(entries)

            if args.pack:
                shallow_copy_images(folder.path, output)

    (failed, passed, identical, failed_str, passed_str, identical_str, missing_golden_str, missing_candidate_str, device_summary_str, device_number) = sort_entries(all_entries)

    to_clean = []
    to_check = []
    # choose who to clean and who to check against
    if args.clean:
        to_clean = identical
        to_check = failed + passed

    if args.fails_only:
        to_clean = identical + passed
        to_check = failed

    # clean them
    for obj in to_clean:
        obj.clean()

    # only remove goldens that are in to_clean and not to_check so that we keep goldens that are still used
    # this part is why we needed __eq__ and __hash__ in our TestEntry class
    for obj in set(to_clean) - set(to_check):
        golden_file = os.path.join(golden_path, f"{obj.name}.png")
        if args.verbose:
            print("deleting orphaned golden " + golden_file)
        os.remove(golden_file)
        if args.pack:
            # remember to remove the original if we packed it
            os.remove(os.path.join(args.goldens, f"{obj.name}.png"))


    write_html(TEMPLATE_PATH, failed_str, passed_str, identical_str, missing_golden_str, missing_candidate_str, device_summary_str, device_number, output_path)

    print(f"total entries {len(all_entries)}")
    write_min_csv(len(passed), len(failed), len(missing_candidate_str), len(missing_golden_str), len(identical), len(all_entries), output_path + "/issues.csv")

def main(argv=None):    
    if not os.path.exists(args.goldens):
        print("Can't find goldens " + args.goldens)
        return -1
    if not os.path.exists(args.candidates):
        print("Can't find candidates " + args.candidates)
        return -1

    # delete output dir if exists
    shutil.rmtree(args.output, ignore_errors=True)
    # remake output dir, this will make it correctly
    # even if it requires creating mulltiple directories
    os.makedirs(args.output, exist_ok=True)

#   reset our scratch files
    for status_filename in glob.iglob(status_filename_pattern):
        os.remove(status_filename)

    TestEntry.load_templates(TEMPLATE_PATH)

    if args.recursive:
        diff_directory_deep(args.candidates, args.output)
    else:
        (entries, missing, success) = diff_directory_shallow(args.candidates, args.output, args.goldens)
        if len(entries) > 0:
            (failed, passed, identical, failed_str, passed_str, identical_str, missing_golden_str, missing_candidate_str, device_summary_str, device_number) = sort_entries(entries)
            assert(len(failed) + len(passed) + len(identical) + len(missing_candidate_str) + len(missing_golden_str) == len(entries))
            write_html(TEMPLATE_PATH, failed_str, passed_str, identical_str, missing_golden_str, missing_candidate_str, device_summary_str, device_number, args.output)
            # note could add these to the html output but w/e
            missing_candidates = [os.path.basename(entry.candidates_path_abs) for entry in missing if entry.type == 'missing_candidate']
            write_csv(entries, args.goldens, args.candidates, args.output, missing_candidates)
            print("Found", len(entries) - len(identical), "differences,",
                  len(failed), "failing.")

            # here we have to do a lot less work than the diff_directory_deep since we know goldens are not shared with other TestEntries
            if args.fails_only:
                for obj in identical+passed:
                    obj.clean()
                    golden_path = os.path.join(args.goldens, f"{obj.name}.png")
                    if args.verbose:
                        print(f"deleting orphaned golden {golden_path}")
                    os.remove(golden_path)
                    
            elif args.clean:
                for obj in identical:
                    obj.clean()
                    golden_path = os.path.join(args.goldens, f"{obj.name}.png")
                    if args.verbose:
                        print(f"deleting orphaned golden {golden_path}")
                    os.remove(golden_path)
                    
            
        # if we are in fail only mode than make it succesful when there are only "passing" entries
        if args.fails_only:
            if failed:
                # if there were diffs, its gotta fail
                print("FAILED.")
                return -1
        # otherwise fail like normal
        elif not success:
            # if there were diffs, its gotta fail
            print("FAILED.")
            return -1
        
    return 0
    
if __name__ == "__main__":
    sys.exit(main())
