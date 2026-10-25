#!/usr/bin/env python3
"""Fix up dots.xcodeproj after the openFrameworks project generator writes it.

- Takes out the flam3 sources dots doesn't use: flam3's command-line tools
  (each has its own main()), its JPEG/PNG writers, and rect.c, which flam3.c
  #includes rather than compiling on its own.
- Links libxml2, which flam3 parses genomes with.
- Points the Syphon framework flags at the app's local addons/ofxSyphon; the
  generator writes them as if ofxSyphon were in the OF addons folder.
- Adds libprojectM (libs/projectM, built by scripts/build_projectm.sh).
- Links JavaScriptCore, which runs Synesthesia scenes' script.js.

Run from the app folder after every projectGenerator run:

    python3 scripts/postgen_xcodeproj.py dots.xcodeproj/project.pbxproj
"""
import json
import sys

EXCLUDE = {
    "flam3-animate.c", "flam3-convert.c", "flam3-genome.c", "flam3-render.c",
    "jpeg.c", "png.c", "rect.c",
}
SYPHON_FLAG = "/addons/ofxSyphon/libs/Syphon/lib/osx/"
LOCAL_SYPHON_FLAG = "-Faddons/ofxSyphon/libs/Syphon/lib/osx/"
PROJECTM_HEADERS = "libs/projectM/include"
PROJECTM_LIBS = ["libs/projectM/lib/libprojectM-4-playlist.a", "libs/projectM/lib/libprojectM-4.a"]


def fix_flags(flags):
    if isinstance(flags, str):
        flags = [flags]
    flags = [LOCAL_SYPHON_FLAG if f.startswith("-F") and f.endswith(SYPHON_FLAG) else f
             for f in flags]
    return flags


def main(path):
    with open(path) as f:
        proj = json.load(f)
    objects = proj["objects"]

    refs = {key for key, obj in objects.items()
            if obj.get("isa") == "PBXFileReference"
            and (obj.get("name") or obj.get("path", "").split("/")[-1]) in EXCLUDE}
    build_files = {key for key, obj in objects.items()
                   if obj.get("isa") == "PBXBuildFile" and obj.get("fileRef") in refs}

    for obj in objects.values():
        if obj.get("isa") == "PBXSourcesBuildPhase":
            obj["files"] = [f for f in obj["files"] if f not in build_files]
    for key in build_files:
        del objects[key]

    n_configs = 0
    for obj in objects.values():
        if obj.get("isa") != "XCBuildConfiguration":
            continue
        settings = obj["buildSettings"]
        if "OTHER_LDFLAGS" not in settings:
            continue
        ldflags = fix_flags(settings["OTHER_LDFLAGS"])
        if "-lxml2" not in ldflags:
            ldflags.append("-lxml2")
        for lib in PROJECTM_LIBS:
            if lib not in ldflags:
                ldflags.append(lib)
        if "JavaScriptCore" not in ldflags:
            ldflags += ["-framework", "JavaScriptCore"]
        settings["OTHER_LDFLAGS"] = ldflags
        headers = settings.get("HEADER_SEARCH_PATHS", ["$(inherited)"])
        if isinstance(headers, str):
            headers = [headers]
        if PROJECTM_HEADERS not in headers:
            headers.append(PROJECTM_HEADERS)
        settings["HEADER_SEARCH_PATHS"] = headers
        if "OTHER_CFLAGS" in settings:
            settings["OTHER_CFLAGS"] = fix_flags(settings["OTHER_CFLAGS"])
        n_configs += 1

    with open(path, "w") as f:
        json.dump(proj, f, indent="\t", sort_keys=True)
        f.write("\n")
    print(f"removed {len(build_files)} build entries for {len(refs)} files; "
          f"fixed link flags in {n_configs} configurations")


if __name__ == "__main__":
    main(sys.argv[1])
