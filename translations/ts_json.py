# SPDX-FileCopyrightText: 2026 grxtor
# SPDX-License-Identifier: Apache-2.0
"""Move unfinished messages between a Qt .ts file and JSON work files.

  export <ts> <out.json> <group>   group: qml | core | widgets | all
  import <ts> <in.json>...         writes the translations back into <ts>
  check <in.json>...               reports placeholder, tag and punctuation slips

A work file is a list of {"id", "context", "source", "comment",
"extracomment", "numerus", "file", "translation"}; "translation" is a
string, or for numerus messages a list of forms. "id" is the message's
position in the .ts, so import needs the same, unchanged .ts.
"""
import json
import sys
import xml.etree.ElementTree as ET


def messages(root):
    i = 0
    for ctx in root.iter("context"):
        name = ctx.findtext("name")
        last = ""
        for m in ctx.iter("message"):
            f = ""
            for loc in m.findall("location"):
                if loc.get("filename"):
                    last = loc.get("filename")
                f = f or last
            yield i, name, m, f or last
            i += 1


def group_of(path):
    if path.endswith((".qml", ".js")) or "/qml/" in path:
        return "qml"
    if "/ui/" in path:
        return "widgets"
    return "core"


def export(ts, out, group):
    root = ET.parse(ts).getroot()
    items = []
    for i, ctx, m, f in messages(root):
        t = m.find("translation")
        if t is None or t.get("type") != "unfinished":
            continue
        if group != "all" and group_of(f) != group:
            continue
        items.append({
            "id": i, "context": ctx, "source": m.findtext("source"),
            "comment": m.findtext("comment") or "", "extracomment": m.findtext("extracomment") or "",
            "numerus": m.get("numerus") == "yes", "file": f, "translation": None,
        })
    json.dump(items, open(out, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
    print(f"{len(items)} messages -> {out}")


def import_(ts, files):
    tree = ET.parse(ts)
    root = tree.getroot()
    by_id = {}
    for path in files:
        for it in json.load(open(path, encoding="utf-8")):
            if it.get("translation"):
                by_id[it["id"]] = it
    done = 0
    for i, _ctx, m, _f in messages(root):
        it = by_id.get(i)
        if not it or it["source"] != m.findtext("source"):
            continue
        t = m.find("translation")
        for child in list(t):
            t.remove(child)
        t.text = None
        if it["numerus"]:
            for form in it["translation"]:
                ET.SubElement(t, "numerusform").text = form
        else:
            t.text = it["translation"]
        t.attrib.pop("type", None)
        done += 1
    tree.write(ts, encoding="utf-8", xml_declaration=True)
    print(f"{done} translations written into {ts}")


def check(files):
    """Placeholders, tags, accelerators and edge punctuation must survive."""
    import re
    ph = re.compile(r"%L?\d|%n")
    tag = re.compile(r"</?[a-zA-Z][^>]*>")
    bad = missing = 0
    for path in files:
        for it in json.load(open(path, encoding="utf-8")):
            tr = it.get("translation")
            if not tr:
                missing += 1
                continue
            forms = tr if isinstance(tr, list) else [tr]
            src = it["source"]
            problems = []
            if it["numerus"] and not isinstance(tr, list):
                problems.append("numerus needs a list")
            for f in forms:
                if sorted(ph.findall(f)) != sorted(ph.findall(src)):
                    problems.append("placeholders")
                if sorted(tag.findall(f)) != sorted(tag.findall(src)):
                    problems.append("tags")
                if ("&" in src.replace("&&", "").replace("&amp;", "")) != ("&" in f.replace("&&", "").replace("&amp;", "")):
                    problems.append("accelerator")
                for edge in (":", "…", "...", " "):
                    if src.endswith(edge) != f.endswith(edge):
                        problems.append(f"ends with {edge!r}")
            if problems:
                bad += 1
                print(f"{path}:{it['id']}: {sorted(set(problems))}\n  src: {src!r}\n  tr:  {tr!r}")
    print(f"{bad} with problems, {missing} untranslated")


if __name__ == "__main__":
    if sys.argv[1] == "export":
        export(sys.argv[2], sys.argv[3], sys.argv[4])
    elif sys.argv[1] == "check":
        check(sys.argv[2:])
    else:
        import_(sys.argv[2], sys.argv[3:])
