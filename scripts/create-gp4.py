#!/usr/bin/env python3
"""Generate a .gp4 project for PkgTool.Core.

The create-gp4 binary bundled with the OpenOrbis toolchain emits a fixed
directory scaffold (assets/{audio,fonts,images,misc,videos}, sce_sys/about,
sce_module) and ignores any other directories. That breaks projects that use
additional folders (e.g. assets/languages): PkgTool.Core aborts with
"Sequence contains no elements" because a file references a directory that is
missing from <rootdir>.

This generator builds a correct <rootdir> tree from the actual file list, so
any directory layout works. Only directories that actually contain files are
emitted (empty directories also crash PkgTool.Core).
"""

import argparse
import datetime
import sys
import xml.sax.saxutils as sax


def build_tree(paths):
    """Turn a list of "a/b/c.bin" paths into a nested dict of directories."""
    root = {}
    for path in paths:
        parts = path.replace("\\", "/").split("/")
        # Everything but the last component is a directory.
        node = root
        for part in parts[:-1]:
            node = node.setdefault(part, {})
    return root


def emit_dirs(node, indent):
    lines = []
    pad = "\t" * indent
    for name in sorted(node.keys()):
        children = node[name]
        attr = sax.quoteattr(name)
        if children:
            lines.append(f"{pad}<dir targ_name={attr}>")
            lines.extend(emit_dirs(children, indent + 1))
            lines.append(f"{pad}</dir>")
        else:
            lines.append(f"{pad}<dir targ_name={attr} />")
    return lines


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", required=True)
    parser.add_argument("--content-id", required=True, dest="content_id")
    parser.add_argument("--files", required=True, help="space separated file list")
    args = parser.parse_args()

    files = [f for f in args.files.split() if f]
    if not files:
        print("create-gp4.py: no files provided", file=sys.stderr)
        return 1

    tree = build_tree(files)
    ts = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    out = []
    out.append('<?xml version="1.0"?>')
    out.append('<psproject xmlns:xsd="http://www.w3.org/2001/XMLSchema" '
               'xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" '
               'fmt="gp4" version="1000">')
    out.append('\t<volume>')
    out.append('\t\t<volume_type>pkg_ps4_app</volume_type>')
    out.append('\t\t<volume_id>PS4VOLUME</volume_id>')
    out.append(f'\t\t<volume_ts>{ts}</volume_ts>')
    out.append(f'\t\t<package content_id={sax.quoteattr(args.content_id)} '
               'passcode="00000000000000000000000000000000" '
               'storage_type="digital50" app_type="full" />')
    out.append('\t\t<chunk_info chunk_count="1" scenario_count="1">')
    out.append('\t\t\t<chunks>')
    out.append('\t\t\t\t<chunk id="0" layer_no="0" label="Chunk #0" />')
    out.append('\t\t\t</chunks>')
    out.append('\t\t\t<scenarios default_id="0">')
    out.append('\t\t\t\t<scenario id="0" type="sp" initial_chunk_count="1" '
               'label="Scenario #0">0</scenario>')
    out.append('\t\t\t</scenarios>')
    out.append('\t\t</chunk_info>')
    out.append('\t</volume>')
    out.append('\t<files img_no="0">')
    for f in files:
        targ = f.replace("\\", "/")
        out.append(f'\t\t<file targ_path={sax.quoteattr(targ)} '
                   f'orig_path={sax.quoteattr(targ)} />')
    out.append('\t</files>')
    out.append('\t<rootdir>')
    out.extend(emit_dirs(tree, 2))
    out.append('\t</rootdir>')
    out.append('</psproject>')

    with open(args.out, "w", newline="\n") as fh:
        fh.write("\n".join(out) + "\n")

    return 0


if __name__ == "__main__":
    sys.exit(main())
