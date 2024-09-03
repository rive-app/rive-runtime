#!/usr/bin/python

from genericpath import exists
import sys
import subprocess
import os.path
import argparse
import glob
import csv
from multiprocessing import Pool
from functools import partial
from xml.etree import ElementTree as ET
from typing import TypeVar
import shutil
import json

_HtmlElement = TypeVar('_HtmlElement', bound='HtmlElement')

parser = argparse.ArgumentParser(description="Compare two directories of images")
parser.add_argument("--goldens", "-g", required=True, help="INPUT directory of correct images")
parser.add_argument("--candidates", "-c", required=True, help="INPUT directory of candidate images")
parser.add_argument("--output", "-o", required=True, help="OUTPUT directory to store differences")
parser.add_argument("--verbose", "-v", action='store_true', help="enable verbose output")
parser.add_argument("--build", "-b", default='release', choices=['debug', 'release'], help="build configuration")
parser.add_argument("-j", "--jobs", default=1, type=int, help="number of jobs to run in parallel")
parser.add_argument("-r", "--recursive", action='store_true', help="recursively diffs images in \"--candidates\" sub folders against \"--goldens\"")
parser.add_argument("-p", "--pack", action='store_true', help="copy candidates and goldens into output folder along with results")
parser.add_argument("-x", "--clean", action='store_true', help="delete golden and candidate images that are identical")
args = parser.parse_args()

# _winapi.WaitForMultipleObjects only supports 64 handles, which we exceed if we span >61 diff jobs.
args.jobs = min(args.jobs, 61)

class HtmlElement(object):
    def __init__(self, tag):
        self.element = ET.Element(tag)

    def __call__(self, tag) -> _HtmlElement:
        return self.__new_element__(tag)

    def __enter__(self):
        return self

    def __exit__(self, exception_type, exception_value, exception_traceback):
        pass

    def __new_element__(self, tag) -> _HtmlElement:
        element = HtmlElement(tag)
        self.element.append(element.element)
        return element

    @property
    def text(self):
        return self.element.text

    @text.setter
    def text(self, text):
        self.element.text = text

    @property
    def tail(self):
        return self.element.tail
    
    @tail.setter
    def tail(self, value):
        self.element.tail = value

    def add_strong_top_text(self, text):
        with self('strong') as s:
            s.add_attribute("style", "vertical-align:top;horizontal-align:right")
            s.text = text

    def add_attribute(self, attribute_key, attribute_value):
        self.element.attrib[attribute_key] = attribute_value

class HtmlDoc(object):
    def __init__(self, file_path, force_html_entity=False):
        self.root = HtmlElement("html")
        self.file_path = file_path
        self.force_html_entity = force_html_entity

    def __call__(self, tag):
        return self.root.__new_element__(tag)

    def __enter__(self):
        return self

    def __exit__(self, exception_type, exception_value, exception_traceback):
        ET.indent(self.root.element)
        html_string = ET.tostring(self.root.element, method='html')
        if self.force_html_entity:
            html_string = html_string.replace(b'&amp;', b'&')
        with open(self.file_path, 'wb') as f:
            f.write(html_string)

status_filename_base = "_imagediff_status"
status_filename_pattern = f"{status_filename_base}_%i_*.txt" % os.getpid()
show_commands = False

class Usage(Exception):
    def __init__(self, msg):
        self.msg = msg

def is_arg(arg, list):
    for item in list:
        if arg == item:
            return True
    return False

def shallow_copy_images(src, dest):
    file_names = [file for file in os.scandir(src) if file.is_file() and '.png' in file.name]
    for file in file_names:
        shutil.copyfile(file.path, os.path.join(dest, file.name))

def remove_suffix(name, oldsuffix):
    if name.endswith(oldsuffix):
        name = name[:-len(oldsuffix)]
    return name

def call_imagediff(filename, tool, golden, candidate, output, parent_pid):
    cmd = [tool,
           "-n", remove_suffix(filename, ".png"),
           "-g", os.path.join(golden, filename),
           "-c", os.path.join(candidate, filename),
           # Each process writes its own status file in order to avoid race conditions.
           "-s", "%s_%i_%i.txt" % (status_filename_base, parent_pid, os.getpid())]
    if output is not None:
        cmd.extend(["-o", output])
    
    if show_commands:
        str = ""
        for c in cmd:
            str += c + " "
        print(str)
    
    if 0 != subprocess.call(cmd):
        print("Error calling " + cmd[0])
        return -1

def parse_status(candidates_path):
    total_lines = 0
    diff_lines = []
    success = True
    status_files = glob.glob(status_filename_pattern)
    if not status_files:
        print('Not a single status file got written, are you just starting new?')
    for status_filename in status_files:
        for line in open(status_filename, "r").readlines():
            total_lines += 1
            words = line.rstrip().split('\t')
            name = words[0]
            if words[1] == "failed":
                print("Failed to compare " + name)
                success = False
            elif words[1] == "identical":
                if args.clean:
                    for path in [candidates_path, args.goldens]:
                        identical_file = "%s/%s.png" % (path, name)
                        if args.verbose:
                            print("deleting identical file " + identical_file)
                        os.remove(identical_file)
            elif words[1] == "sizemismatch":
                print("Dimensions mismatch for " + name)
                success = False
            else:
                diff_lines.append(words)
                success = False
    return (total_lines, diff_lines, success)

def write_image(html_element: HtmlElement, path, height):
    with html_element('a') as a:
        a.add_attribute("href", path)
        with a('img') as img:
            img.add_attribute('src', path)
            img.add_attribute('height', height)
        #a.tail = '&nbsp;'

def write_row(html_element: HtmlElement, name, origpath, candidatepath, diff_path, height, max_component_diff, ave_component_diff,
              pixel_diffcount, pixeldiff_percent):
    with html_element('h3') as b:
        b.text = name
    
    with html_element("p") as p:
        p.text = f"Max RGB mismatch: {max_component_diff}"
        p.tail = None
        with p('br') as br:
            br.tail = f"Avg RGB mismatch: {ave_component_diff}"
        with p('br') as br:
            br.tail = f"# of different pixels: {pixel_diffcount} ({pixeldiff_percent}%)"
        p('br')
        
    html_element('br')
    with html_element('table') as table:
        table.add_attribute("style", "border-spacing: 10px 0; background-color: #EEE;")
        with table('tr') as tr:
            tr.add_attribute("height", "10px")
        with table('tr') as tr:
            with tr('td') as td:
                td('h4').text = "golden"
            with tr('td') as td:
                td('h4').text = "candidate"
            with tr('td') as td:
                td('h4').text = "color diff"
            with tr('td') as td:
                td('h4').text = "pixel diff"
        with table('tr') as tr:
            with tr('td') as td:
                write_image(td, os.path.join(origpath, name + ".png"), height)
            with tr('td') as td:
                write_image(td, os.path.join(candidatepath, name + ".png"), height)
            with tr('td') as td:
                write_image(td, os.path.join(diff_path, name + ".diff0.png"), height)
            with tr('td') as td:
                write_image(td, os.path.join(diff_path, name + ".diff1.png"), height)
        with table('tr') as tr:
            tr.add_attribute("height", "10px")

def write_row_identical(html_element: HtmlElement, name, origpath, candidatepath, diff_path, height):
    with html_element('h3') as b:
        b.text = name
    
    with html_element("p") as p:
        p.text = f"Golden and Conadidate are identical"
        
    html_element('br')
    with html_element('div') as outer:
        outer.add_attribute("style", "overflow: hidden;background-color: #EEEEEE; overflow:auto;")
        with outer('div') as div:
            div.add_attribute("style", "width: 25%; float: left; margin: auto")
            div('h4').text = "golden"
            write_image(div, os.path.join(origpath, name + ".png"), height)
        with outer('div') as div:
            div.add_attribute("style", "width: 25%; float: left; margin: auto")
            div('h4').text = "candidate"
            write_image(div, os.path.join(candidatepath, name + ".png"), height)

def write_html(rows, origpath, candidatepath, diffpath):
    origpath = os.path.relpath(origpath, diffpath)
    candidatepath = os.path.relpath(candidatepath, diffpath)

    # Sort the rows by ave_component_diff -- larger diffs first.
    rows.sort(reverse=True, key=lambda row: row[2])

    height = '256'
    with HtmlDoc(os.path.join(diffpath, "index.html"), True) as f:
        for row in rows:
            write_row(f, row[0], origpath, candidatepath, "", height, row[1], row[2], row[3],
                    "{:.2f}".format(float(row[3]) * 100 / float(row[4])))
            
def write_column(html, device, rows, origpath, candidatepath, browserstack_details:dict=None):
    with html('h2') as h:
        h.text = device
    if browserstack_details is not None and 'browser_url' in browserstack_details.keys():
        with html('a') as a:
            a.text = 'BrowserStack Link'
            a.add_attribute('href', browserstack_details['browser_url'])
    height = '256'
    rows.sort(reverse=True, key=lambda row: row[2])
    for row in rows:
        if len(row) == 5:
            write_row(html, row[0], origpath, candidatepath, device, height, row[1], row[2], row[3],
                    "{:.2f}".format(float(row[3]) * 100 / float(row[4])))
        else:
            write_row_identical(html, row[0], origpath, candidatepath, device, height)

def write_csv(rows, origpath, candidatepath, diffpath, missing_candidates):
    origpath = os.path.relpath(origpath, diffpath)
    candidatepath = os.path.relpath(candidatepath, diffpath)

    height = 256

    with open(os.path.join(diffpath, "data.csv"), "w") as csvfile:
        fieldnames = ['file_name','original', 'candidate',  'max_rgb', 'avg_rgb', 'pixel_diff_count', 'pixel_diff_percent', 'color_diff', 'pixel_diff']
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        writer.writeheader()
        for name in missing_candidates:
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

        for row in rows:
            name = row[0]
            writer.writerow({
                'file_name': name,
                'original': os.path.join(origpath, name + ".png") , 
                'candidate': os.path.join(candidatepath, name + ".png"),
                'max_rgb':row[1],
                'avg_rgb':row[2],
                'pixel_diff_count': row[3],
                'pixel_diff_percent': "{:.2f}".format(float(row[3]) * 100 / float(row[4])),
                'color_diff': name + ".diff0.png",
                'pixel_diff': name + ".diff1.png"
            })

def write_min_csv(columns, csv_path):

    # delete and old data
    if os.path.exists(csv_path):
        os.remove(csv_path)
    
    total_vector = 0
    total_vector_issues = 0

    total_mesh = 0
    total_mesh_issues = 0

    total_text = 0
    total_text_issues = 0

    for column in columns.values():
        for row in column['diff_lines']:
            if row[0] == 'vector':
                total_vector += 1
                if float(row[2]) > 0.01:
                    total_vector_issues += 1
            if row[0] == 'mesh':
                total_mesh += 1
                if float(row[2]) > 0.01:
                    total_mesh_issues += 1
            if row[0] == 'text':
                total_text += 1
                if float(row[2]) > 0.01:
                    total_text_issues += 1

    with open(csv_path, 'w', newline='') as csv_file:
        csv_writer = csv.DictWriter(csv_file, fieldnames=['type', 'total_issues', "serious_issues"])
        csv_writer.writerow({'type':'vector', 'total_issues' : str(total_vector), 'serious_issues' : str(total_vector_issues)})
        csv_writer.writerow({'type':'mesh', 'total_issues' : str(total_mesh), 'serious_issues' : str(total_mesh_issues)})
        csv_writer.writerow({'type':'text', 'total_issues' : str(total_text), 'serious_issues' : str(total_text_issues)})

def diff_directory_shallow(tool_path, candidates_path, output_path):
    original_filenames = set((file.name for file in os.scandir(candidates_path) if file.is_file()))
    candidate_filenames = set(os.listdir(args.goldens))
    intersect_filenames = original_filenames.intersection(candidate_filenames)

    missing_candidates = []
    for file in original_filenames.difference(candidate_filenames):
        print(f'Candidate file {file} missing in goldens.')
        missing_candidates.append(file)

    for file in candidate_filenames.difference(original_filenames):
        print(f'Golden file {file} missing in candidates.')
    if args.jobs > 1:
        print("Diffing %i candidates in %i processes..." % (len(intersect_filenames), args.jobs))
    else:
        print("Diffing %i candidates..." % len(intersect_filenames))
    sys.stdout.flush()

#   generate the diffs (if any) and write to the status file
    f = partial(call_imagediff,
                tool=tool_path,
                golden=args.goldens,
                candidate=candidates_path,
                output=output_path,
                parent_pid=os.getpid())
    Pool(args.jobs).map(f, intersect_filenames)

    (total_lines, diff_lines, success) = parse_status(candidates_path)
    
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

    return (diff_lines, missing_candidates, success)

def diff_directory_deep(tool_path, candidates_path, output_path):
    if args.pack:
        new_golden_path = os.path.join(output_path, "golden")
        os.makedirs(new_golden_path, exist_ok=True)
        shallow_copy_images(args.goldens, new_golden_path)
        args.goldens = new_golden_path
    
    diff_columns = {}

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
            
            (diff_lines, _, _) = diff_directory_shallow(tool_path, folder.path, output)
            
            if args.pack:
                shallow_copy_images(folder.path, output)
                if diff_lines:
                    diff_columns[folder.name] = {'diff_lines': diff_lines, 'input_path': folder.name, 'session_details' : browserstack_details}
            else:
                if diff_lines:
                    diff_columns[folder.name] = {'diff_lines': diff_lines, 'input_path': folder.path, 'session_details' : browserstack_details}
    


    diff_list = sorted(diff_columns.items(), key = column_sorter, reverse=True)

    if args.pack:
        args.goldens = 'golden'

    with HtmlDoc(output_path + "/index.html", True) as html:
        html('header')
        with html('body') as body:
            body.add_attribute('style', 'background-color: ##e6e6e6')
            for device in diff_list:
                write_column(body, device[0], device[1]['diff_lines'], args.goldens, device[1]['input_path'], device[1]['session_details'])
            

    print(f"total columns {len(diff_columns)}")
    write_min_csv(diff_columns, output_path + "/issues.csv")

def column_sorter(in_item):
    rows = in_item[1]['diff_lines']
    return max([x[2] for x in rows])

def main(argv=None):
    tool = "out/" + args.build + "/imagediff"
    if args.verbose:
        print("Using '" + tool + "'")
    
    if not os.path.exists(args.goldens):
        print("Can't find goldens " + args.goldens)
        return -1;
    if not os.path.exists(args.candidates):
        print("Can't find candidates " + args.candidates)
        return -1;

    # delete output dir if exists
    shutil.rmtree(args.output, ignore_errors=True)
    # remake output dir
    os.mkdir(args.output)

#   reset our scratch files
    for status_filename in glob.iglob(status_filename_pattern):
        os.remove(status_filename)

    if args.recursive:
        diff_directory_deep(tool, args.candidates, args.output)
    else:
        (diff_lines, missing_candidates, success) = diff_directory_shallow(tool, args.candidates, args.output)
        if len(diff_lines) > 0:
            write_html(diff_lines, args.goldens, args.candidates, args.output)
            # note could add these to the html output but w/e
            write_csv(diff_lines, args.goldens, args.candidates, args.output, missing_candidates)
            print("Found " + str(len(diff_lines)) + " differences.")
        if not success:
            # if there were diffs, its gotta fail
            print("FAILED.")
            return -1
        
    return 0
    
if __name__ == "__main__":
    sys.exit(main())
