#!/usr/bin/env python3

"""
A tool to generate that part of config.json that deals with pages and fields.

Usage example:

 1. Delete all lines from config.json from the curly backet before
    "name": "page1type"  to the end of the file

 2. run ./gen_set.py -d obp60 -p 10 >> config.json

TODO Better handling of default pages

"""

import os
import sys
import getopt
import re
import json

__version__ = "0.2"

def detect_pages(filename):
    # returns a dictionary with page name and the number of gui fields
    pagefiles = []
    with open(filename, 'r') as fh:
        pattern = r'extern PageDescription\s*register(Page[^;\s]*)'
        for line in fh:
            if "extern PageDescription" in line:
                match = re.search(pattern, line)
                if match:
                    pagefiles.append(match.group(1))
    try:
        pagefiles.remove('PageSystem')
    except ValueError:
        pass
    pagedata = {}
    for pf in pagefiles:
        filename = pf + ".cpp"
        with open(filename, 'r') as fh:
            content = fh.read()
        pattern =  r'PageDescription\s*?register' + pf + r'\s*\(\s*"([^"]+)".*?\n\s*(\d+)'
        match = re.search(pattern, content, re.DOTALL)
        if match:
            pagedata[match.group(1)] = int(match.group(2))
    return pagedata

def get_default_page(pageno):
    # Default selection for each page
    default_pages = (
        "Voltage",
        "WindRose",
        "OneValue",
        "TwoValues",
        "ThreeValues",
        "FourValues",
        "FourValues2",
        "Clock",
        "RollPitch",
        "Battery2"
    )
    if pageno > len(default_pages):
        return "OneValue"
    return default_pages[pageno - 1]

def number_to_text(number):
    if number < 0 or number > 99:
        raise ValueError("Only numbers from 0 to 99 are allowed.")
    numbers = ("zero", "one", "two", "three", "four", "five", "six", "seven",
        "eight", "nine", "ten", "eleven", "twelve", "thirteen", "fourteen",
        "fifteen", "sixteen", "seventeen", "eighteen", "nineteen")
    tens = ("", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy",
        "eighty", "ninety")
    if number < 20:
        return numbers[number]
    else:
        q, r = divmod(number, 10)
        return tens[q] + numbers[r]

def create_json(device, no_of_pages, pagedata):

    pages = sorted(pagedata.keys())
    max_no_of_fields_per_page = max(pagedata.values())

    output = []

    for page_no in range(1, no_of_pages + 1):
        page_data = {
            "name": f"page{page_no}type",
            "label": "Type",
            "type": "list",
            "default": get_default_page(page_no),
            "description": f"Type of page for page {page_no}",
            "list": pages,
            "category": f"{device.upper()} Page {page_no}",
            "capabilities": {device.lower(): "true"},
            "condition": [{"visiblePages": vp} for vp in range(page_no, no_of_pages + 1)],
            #"fields": [],
        }
        output.append(page_data)

        for field_no in range(1, max_no_of_fields_per_page + 1):
            field_data = {
                "name": f"page{page_no}value{field_no}",
                "label": f"Field {field_no}",
                "type": "boatData",
                "default": "",
                "description": "The display for field {}".format(number_to_text(field_no)),
                "category": f"{device.upper()} Page {page_no}",
                "capabilities": {device.lower(): "true"},
                "condition": [
                    {f"page{page_no}type": page}
                    for page in pages
                    if pagedata[page] >= field_no
                ],
            }
            output.append(field_data)

        fluid_data ={
            "name": f"page{page_no}fluid",
            "label": "Fluid type",
            "type": "list",
            "default": "0",
            "list": [
            {"l":"Fuel (0)","v":"0"},
            {"l":"Water (1)","v":"1"},
            {"l":"Gray Water (2)","v":"2"},
            {"l":"Live Well (3)","v":"3"},
            {"l":"Oil (4)","v":"4"},
            {"l":"Black Water (5)","v":"5"},
            {"l":"Fuel Gasoline (6)","v":"6"}
            ],
            "description": "Fluid type in tank",
            "category": f"{device.upper()} Page {page_no}",
            "capabilities": {
                device.lower(): "true"
            },
            "condition":[{f"page{page_no}type":"Fluid"}]
            }
        output.append(fluid_data)

    return json.dumps(output, indent=4)

def usage():
    print("{} v{}".format(os.path.basename(__file__), __version__))
    print()
    print("Command line options")
    print(" -d --device     device name to use e.g. obp60")
    print(" -p --pages      number of pages to create")
    print(" -h              show this help")
    print()

if __name__ == '__main__':
    try:
        options, remainder = getopt.getopt(sys.argv[1:], 'd:p:', ['device=','--pages='])
    except getopt.GetoptError as err:
        print(err)
        usage()
        sys.exit(2)

    device = "obp60"
    no_of_pages = 10
    for opt, arg in options:
        if opt in ('-d', '--device'):
            device = arg
        elif opt in ('-p', '--pages'):
            no_of_pages = int(arg)
        elif opt == '-h':
            usage()
            sys.exit(0)

    # automatic detect pages and number of fields from sourcecode
    pagedata = detect_pages("obp60task.cpp")

    json_output = create_json(device, no_of_pages, pagedata)
    # print omitting first line containing [  of JSON array
    print(json_output[1:])
