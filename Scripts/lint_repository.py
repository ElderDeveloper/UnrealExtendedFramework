#!/usr/bin/env python3
# Copyright Moon Punch Games. All Rights Reserved.
"""Fast structural checks for the Extended plugin suite. Needs no Unreal install.

    python3 Scripts/lint_repository.py          # all checks, exit 1 on errors
    python3 Scripts/lint_repository.py --strict # warnings fail too

Checks:
  plugins     every .uplugin is valid JSON and every module it lists has a Build.cs
  plugin-deps a module that depends on a module from another plugin of this repository lists that
              plugin in its .uplugin
  includes    a file that includes a header from another module of this repository declares that
              module in its Build.cs
  headers     every source file starts with a copyright line; no Unreal placeholder notice
  redirects   every CoreRedirects entry has an OldName and a NewName
  templates   files under Templates/ end in .template, so UnrealBuildTool never loads them
"""

import argparse
import json
import re
import sys
from collections import defaultdict
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
SOURCE_SUFFIXES = {".h", ".hpp", ".cpp", ".inl", ".cs"}
# Vendored SDK code, third-party code and tool-generated files keep their own headers and are not checked.
SKIPPED_PARTS = {"ThirdParty", "Thirdparty", "Generated", "Intermediate", "Binaries", "Saved", ".git", ".claude", "Templates"}
INCLUDE = re.compile(r'^\s*#\s*include\s*"([^"]+)"', re.M)
STRING_LITERAL = re.compile(r'"([A-Za-z0-9_]+)"')


class Report:
    def __init__(self):
        self.errors = []
        self.warnings = []

    def error(self, check, message):
        self.errors.append(f"[{check}] {message}")

    def warn(self, check, message):
        self.warnings.append(f"[{check}] {message}")


def is_skipped(path):
    return any(part in SKIPPED_PARTS for part in path.relative_to(REPO_ROOT).parts)


def rel(path):
    return path.relative_to(REPO_ROOT).as_posix()


def load_plugins(report):
    """Returns {plugin_name: {"dir": Path, "descriptor": dict, "modules": [names]}}."""
    plugins = {}
    for descriptor_path in sorted(REPO_ROOT.glob("*/*.uplugin")):
        try:
            descriptor = json.loads(descriptor_path.read_text(encoding="utf-8-sig"))
        except json.JSONDecodeError as error:
            report.error("plugins", f"{rel(descriptor_path)} is not valid JSON: {error}")
            continue
        plugin_dir = descriptor_path.parent
        modules = [module.get("Name", "") for module in descriptor.get("Modules", [])]
        for module in modules:
            build_cs = plugin_dir / "Source" / module / f"{module}.Build.cs"
            if not build_cs.exists():
                # Some modules live one folder deeper (e.g. Source/ThirdParty/<Module>).
                matches = list(plugin_dir.glob(f"Source/**/{module}.Build.cs"))
                if not matches:
                    report.error("plugins", f"{rel(descriptor_path)} lists module {module} but no {module}.Build.cs exists")
        plugins[descriptor_path.stem] = {"dir": plugin_dir, "descriptor": descriptor, "modules": modules}
    return plugins


def find_modules(plugins):
    """Returns {module_name: {"plugin": name, "dir": Path, "build_cs": Path, "deps": set}}."""
    modules = {}
    for plugin_name, plugin in plugins.items():
        for build_cs in plugin["dir"].glob("Source/**/*.Build.cs"):
            module_name = build_cs.name[: -len(".Build.cs")]
            text = build_cs.read_text(encoding="utf-8-sig")
            # Every quoted identifier in the rules file: dependency lists, include path modules.
            dependencies = set(STRING_LITERAL.findall(text))
            modules[module_name] = {"plugin": plugin_name, "dir": build_cs.parent, "build_cs": build_cs, "deps": dependencies}
    return modules


def check_plugin_dependencies(plugins, modules, report):
    for module_name, module in modules.items():
        own_plugin = module["plugin"]
        listed = {entry.get("Name") for entry in plugins[own_plugin]["descriptor"].get("Plugins", [])}
        for dependency in module["deps"]:
            if dependency == module_name or dependency not in modules:
                continue
            other_plugin = modules[dependency]["plugin"]
            if other_plugin != own_plugin and other_plugin not in listed:
                report.error("plugin-deps", f"{module_name} ({own_plugin}) depends on {dependency} from {other_plugin}, "
                                            f"but {own_plugin}.uplugin does not list {other_plugin}")


def index_headers(modules):
    """Maps include spellings to the modules that provide them."""
    providers = defaultdict(set)
    for module_name, module in modules.items():
        module_dir = module["dir"]
        for header in module_dir.rglob("*.h"):
            if is_skipped(header):
                continue
            relative_to_module = header.relative_to(module_dir)
            parts = relative_to_module.parts
            if parts and parts[0] in ("Public", "Classes"):
                providers[Path(*parts[1:]).as_posix()].add(module_name)
            elif parts and parts[0] == "Private":
                continue
            else:
                # Legacy layout: module root and the Source folder above it are on the include path.
                providers[relative_to_module.as_posix()].add(module_name)
            providers[(Path(module_name) / relative_to_module).as_posix()].add(module_name)
    return providers


def owning_module(path, modules):
    best = None
    for module_name, module in modules.items():
        try:
            path.relative_to(module["dir"])
        except ValueError:
            continue
        if best is None or len(module["dir"].parts) > len(modules[best]["dir"].parts):
            best = module_name
    return best


def check_includes(modules, report):
    providers = index_headers(modules)
    for module_name, module in modules.items():
        for source in module["dir"].rglob("*"):
            if source.suffix not in {".h", ".cpp", ".inl"} or is_skipped(source):
                continue
            if owning_module(source, modules) != module_name:
                continue
            text = source.read_text(encoding="utf-8-sig", errors="replace")
            for included in INCLUDE.findall(text):
                if included.endswith(".generated.h"):
                    continue
                if (source.parent / included).exists():
                    continue  # relative to the including file
                owners = providers.get(included)
                if not owners or module_name in owners:
                    continue
                if not owners & module["deps"]:
                    report.error("includes", f"{rel(source)} includes \"{included}\" from {', '.join(sorted(owners))}, "
                                             f"which {module_name}.Build.cs does not list")


def check_headers(report):
    for path in sorted(REPO_ROOT.glob("*/Source/**/*")):
        if path.suffix not in SOURCE_SUFFIXES or is_skipped(path):
            continue
        text = path.read_text(encoding="utf-8-sig", errors="replace")
        head = "\n".join(text.splitlines()[:3])
        if "Fill out your copyright notice" in text[:400]:
            report.error("headers", f"{rel(path)} still has the Unreal placeholder copyright notice")
        elif "Copyright" not in head:
            report.error("headers", f"{rel(path)} has no copyright line in its first three lines")


REDIRECT_LINE = re.compile(r"^\+?(Class|Struct|Enum|Function|Property|Package)Redirects=\((.*)\)\s*$")


def check_redirects(report):
    for ini in sorted(REPO_ROOT.glob("*/Config/*.ini")):
        in_section = False
        for number, line in enumerate(ini.read_text(encoding="utf-8-sig").splitlines(), start=1):
            stripped = line.strip()
            if stripped.startswith("["):
                in_section = stripped == "[CoreRedirects]"
                continue
            if not in_section or not stripped or stripped.startswith(";"):
                continue
            match = REDIRECT_LINE.match(stripped)
            if not match:
                report.error("redirects", f"{rel(ini)}:{number} is not a recognised redirect: {stripped}")
                continue
            body = match.group(2)
            if "OldName=" not in body or ("NewName=" not in body and "MatchSubstring" not in body):
                report.error("redirects", f"{rel(ini)}:{number} needs both OldName and NewName")


def check_templates(report):
    template_root = REPO_ROOT / "Templates"
    if not template_root.exists():
        return
    for path in template_root.rglob("*"):
        if path.is_file() and path.suffix != ".template":
            report.error("templates", f"{rel(path)} must end in .template so UnrealBuildTool ignores it")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--strict", action="store_true", help="treat warnings as errors")
    args = parser.parse_args()

    report = Report()
    plugins = load_plugins(report)
    modules = find_modules(plugins)
    check_plugin_dependencies(plugins, modules, report)
    check_includes(modules, report)
    check_headers(report)
    check_redirects(report)
    check_templates(report)

    for warning in report.warnings:
        print(f"warning: {warning}")
    for error in report.errors:
        print(f"error: {error}")

    failed = report.errors or (args.strict and report.warnings)
    print(f"{len(plugins)} plugins, {len(modules)} modules: {len(report.errors)} error(s), {len(report.warnings)} warning(s)")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
