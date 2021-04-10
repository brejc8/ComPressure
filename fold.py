#!/usr/bin/python3

import fileinput, textwrap

dialogue_count = 0
first_line = True
for line_in in fileinput.input():
    name = fileinput.filename().replace(".txt","")
    if line_in.rstrip().startswith("#"):
        continue
    if first_line:
        if not line_in.startswith("!"):
            continue
        print ("static Dialogue " + name + str(dialogue_count) + "[] = {")
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
        print ('        "' + line_in.replace('"','\\"') + '\\n"')


print ('Dialogue* ' + name + '[] = {', end ="")
for x in range(dialogue_count):
    print (name + str(x) + ", ", end ="")
print ('NULL};')
