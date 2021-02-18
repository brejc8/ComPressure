#!/usr/bin/python

import fileinput, textwrap

dialogue_count = 0
first_line = True
for line_in in fileinput.input():
    if line_in.rstrip().startswith("#"):
        continue
    if first_line:
        if not line_in.startswith("!"):
            continue
        print ("static Dialogue " + fileinput.filename().replace(".txt","") + str(dialogue_count) + "[] = {")
    line_in = line_in.rstrip()
    if line_in.startswith("!"):
        if not first_line:
            print('    },\n');
        if line_in == "!END":
            print('    {DIALOGUE_END, NULL}\n};\n');
            first_line = True
            dialogue_count += 1
        else: 
            first_line = False
            print("    {DIALOGUE_" + line_in[1:] + ',');
    else:
        if line_in == "":
            print ('        "\\n"')
        else:
            for line_out in textwrap.wrap(line_in, 62):
                line_out = line_out.replace('"','\\"')
                print ('        "' + line_out + '\\n"')


print ('Dialogue* ' + fileinput.filename().replace(".txt","") + '[] = {', end ="")
for x in range(dialogue_count):
    print ("dialogue" + str(x) + ", ", end ="")
print ('NULL};')
