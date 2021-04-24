#!/usr/bin/python3

import fileinput, textwrap

for line_in in fileinput.input():
    line_in = line_in.rstrip()
    print ('        "' + line_in.replace('"','\\"') + '\\n"')
