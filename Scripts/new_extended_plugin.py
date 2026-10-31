#!/usr/bin/env python3
# Copyright Moon Punch Games. All Rights Reserved.
"""Create a new Extended plugin from Templates/ExtendedPlugin.

    python3 Scripts/new_extended_plugin.py Inventory --prefix EInv \
        --description "Inventories, items and loot tables."

creates UnrealExtendedInventory/ with an ExtendedInventory runtime module that depends on
UnrealExtendedFoundation, a settings page, an example subsystem and async node, an offline test,
a README and a CoreRedirects config, all following ARCHITECTURE.md.
"""

import argparse
import re
import shutil
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
TEMPLATE_ROOT = REPO_ROOT / "Templates" / "ExtendedPlugin"
FOUNDATION_VERSION_HEADER = REPO_ROOT / "UnrealExtendedFoundation" / "Source" / "ExtendedFoundation" / "Public" / "EFFoundationVersion.h"
ICON = REPO_ROOT / "UnrealExtendedFoundation" / "Resources" / "Icon128.png"

IDENTIFIER = re.compile(r"^[A-Z][A-Za-z0-9]*$")


def foundation_version():
    text = FOUNDATION_VERSION_HEADER.read_text(encoding="utf-8")
    major = re.search(r"#define EF_FOUNDATION_VERSION_MAJOR (\d+)", text).group(1)
    minor = re.search(r"#define EF_FOUNDATION_VERSION_MINOR (\d+)", text).group(1)
    return major, minor


def split_words(name):
    return re.sub(r"(?<=[a-z0-9])(?=[A-Z])", " ", name)


def build_values(args):
    short = args.name
    plugin = f"UnrealExtended{short}"
    module = f"Extended{short}"
    major, minor = foundation_version()
    return {
        "ShortName": short,
        "PluginName": plugin,
        "ModuleName": module,
        "Prefix": args.prefix,
        "API": module.upper() + "_API",
        "LogCategory": f"Log{module}",
        "FriendlyName": f"Unreal Extended {split_words(short)}",
        "Description": args.description or f"Extended {split_words(short)} plugin.",
        "Author": args.author,
        "Copyright": f"Copyright {args.copyright_holder}. All Rights Reserved.",
        "FoundationMajor": major,
        "FoundationMinor": minor,
    }


def substitute(text, values):
    def replace(match):
        key = match.group(1)
        if key not in values:
            raise KeyError(f"Template uses unknown placeholder {{{{{key}}}}}")
        return values[key]

    return re.sub(r"\{\{(\w+)\}\}", replace, text)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("name", help="Short PascalCase name, e.g. Inventory -> UnrealExtendedInventory / ExtendedInventory")
    parser.add_argument("--prefix", required=True, help="Type prefix without U/F/E, e.g. EInv -> UEInvSubsystem")
    parser.add_argument("--description", default="", help="One sentence for the .uplugin and README")
    parser.add_argument("--author", default="Kemal Erdem YILMAZ")
    parser.add_argument("--copyright-holder", default="Kemal Erdem YILMAZ")
    parser.add_argument("--output", default=str(REPO_ROOT), help="Folder to create the plugin in (default: repository root)")
    args = parser.parse_args()

    for label, value in (("name", args.name), ("prefix", args.prefix)):
        if not IDENTIFIER.match(value):
            parser.error(f"{label} must be PascalCase letters and digits, got {value!r}")

    values = build_values(args)
    destination = Path(args.output).resolve() / values["PluginName"]
    if destination.exists():
        parser.error(f"{destination} already exists")

    created = []
    for source in sorted(TEMPLATE_ROOT.rglob("*")):
        if source.is_dir():
            continue
        relative = source.relative_to(TEMPLATE_ROOT)
        target_relative = Path(substitute(str(relative), values))
        if target_relative.suffix == ".template":
            target_relative = target_relative.with_suffix("")
        target = destination / target_relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(substitute(source.read_text(encoding="utf-8"), values), encoding="utf-8")
        created.append(target)

    (destination / "Resources").mkdir(parents=True, exist_ok=True)
    shutil.copyfile(ICON, destination / "Resources" / "Icon128.png")
    created.append(destination / "Resources" / "Icon128.png")

    print(f"Created {values['PluginName']} ({len(created)} files) in {destination}")
    print("Next steps:")
    print(f"  1. Add {values['PluginName']} to the suite table and dependency table in README.md.")
    print(f"  2. Enable it in your .uproject and build; run 'Automation RunTests {values['PluginName']}'.")
    print("  3. Run python3 Scripts/lint_repository.py before committing.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
