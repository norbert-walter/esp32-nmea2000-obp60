# PlatformIO extra script for obp60task

import subprocess

patching = False

epdtype = "unknown"
pcbvers = "unknown"
for x in env["BUILD_FLAGS"]:
    if not x.startswith('-D'):
        continue
    opt = x[2:].strip()
    if opt.startswith("HARDWARE_"):
        pcbvers = x.split('_')[1]
    elif opt.startswith("DISPLAY_"):
        epdtype = x.split('_')[1]
    elif opt == 'ENABLE_PATCHES':
        patching = True

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

if patching:
    # apply patches to gateway code
    print("applying gateway patches")
    patchdir = os.path.join(os.path.dirname(script), "patches")
    if not os.path.isdir(patchdir):
        print("patchdir not found, no patches applied")
    else:
        patchfiles = [f for f in os.listdir(patchdir)]
        for p in patchfiles:
            patch = os.path.join(patchdir, p)
            print(f"applying {patch}")
            res = subprocess.run(["git", "apply", patch], capture_output=True, text=True)
            if res.returncode != 0:
                print(res.stderr)
        else:
            print("no patches found")
