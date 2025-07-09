# PlatformIO extra script for obp60task

epdtype = "unknown"
pcbvers = "unknown"
for x in env["BUILD_FLAGS"]:
    if x.startswith("-D HARDWARE_"):
        pcbvers = x.split('_')[1]
    if x.startswith("-D DISPLAY_"):
        epdtype = x.split('_')[1]

propfilename = os.path.join(env["PROJECT_LIBDEPS_DIR"], env["PIOENV"], "GxEPD2/library.properties")
properties = {}
with open(propfilename, 'r') as file:
    for line in file:
        match = re.match(r'^([^=]+)=(.*)$', line)
        if match:
            key = match.group(1).strip()
            value = match.group(2).strip()
            properties[key] = value

gxepd2vers = "unknown"
try:
    if properties["name"] == "GxEPD2":
        gxepd2vers = properties["version"]
except:
    pass

env["CPPDEFINES"].extend([("BOARD", env["BOARD"]), ("EPDTYPE", epdtype), ("PCBVERS", pcbvers), ("GXEPD2VERS", gxepd2vers)])

print("added hardware info to CPPDEFINES")
