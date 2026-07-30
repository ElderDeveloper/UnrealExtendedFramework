#!/usr/bin/env node
// Extract the deterministic prototype state without running support.js, React, or a browser.

import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import process from "node:process";
import vm from "node:vm";
import { fileURLToPath } from "node:url";


const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const pluginRoot = path.resolve(scriptDir, "..", "..");
const htmlPath = path.join(pluginRoot, "HTML", "Backlot for UE5.dc.html");
const outputPath = path.join(pluginRoot, "Tests", "Parity", "BacklotFixture.json");
const expectedPayloadSha256 = "BE30EFC1E6338FC411FD905D7B13FE9C07432E142054A1516919504D4AAA67D9";
const html = fs.readFileSync(htmlPath, "utf8");
const scriptMatch = html.match(
  /<script\b[^>]*\bdata-dc-script\b[^>]*>([\s\S]*?)<\/script>\s*<\/body>/i,
);

if (!scriptMatch) {
  throw new Error(`Could not locate data-dc-script in ${htmlPath}`);
}

const context = {
  DCLogic: class {
    constructor(props = {}) {
      this.props = props;
    }
  },
  console,
  setTimeout: () => 0,
  clearTimeout: () => {},
  document: {},
  window: {},
};

vm.createContext(context);
vm.runInContext(
  `${scriptMatch[1]}
globalThis.__backlot = {
  Component,
  S,
  PEOPLE,
  EPICS,
  TYPES,
  VIEWS,
  STATUS_F,
  ASSIGNEE_F,
  PRIOS,
  PTS,
  PIN_KINDS,
  ANNOT,
  BLOCK_MARGIN,
  DEFAULT_HEADS,
  BLOCK_TYPES,
  NEW_BLOCK,
  NO_PIN,
  NO_ISSUE,
  SNIPPET
};`,
  context,
  { filename: htmlPath },
);

const source = context.__backlot;
const component = new source.Component({
  startView: "docs",
  railOpen: true,
  compact: false,
});
const payload = {
  constants: {
    statuses: source.S,
    people: source.PEOPLE,
    epics: source.EPICS,
    types: source.TYPES,
    views: source.VIEWS.map(({ label, dot }) => ({ label, dot })),
    statusFilters: source.STATUS_F,
    assigneeFilters: source.ASSIGNEE_F,
    priorities: source.PRIOS,
    points: source.PTS,
    pinKinds: source.PIN_KINDS,
    annotationColors: source.ANNOT,
    blockMargins: source.BLOCK_MARGIN,
    defaultTableHeaders: source.DEFAULT_HEADS,
    blockTypes: source.BLOCK_TYPES,
    newBlocks: Object.fromEntries(
      Object.entries(source.NEW_BLOCK).map(([kind, factory]) => [kind, factory()]),
    ),
    noPin: source.NO_PIN,
    noIssue: source.NO_ISSUE,
    snippet: source.SNIPPET,
  },
  initialState: component.state,
};
const payloadSha256 = crypto
  .createHash("sha256")
  .update(JSON.stringify(payload))
  .digest("hex")
  .toUpperCase();
if (payloadSha256 !== expectedPayloadSha256) {
  throw new Error(
    `Backlot fixture changed: expected ${expectedPayloadSha256}, got ${payloadSha256}`,
  );
}
const fixture = {
  schemaVersion: 1,
  source: {
    html: path.basename(htmlPath),
    htmlSha256: crypto.createHash("sha256").update(html).digest("hex").toUpperCase(),
    fixturePayloadSha256: payloadSha256,
    authoredViewport: { width: 1920, height: 1080 },
  },
  ...payload,
};

fs.mkdirSync(path.dirname(outputPath), { recursive: true });
fs.writeFileSync(outputPath, `${JSON.stringify(fixture, null, 2)}\n`, "utf8");
console.log(`Wrote ${outputPath}`);
console.log(`Fixture payload SHA-256 ${payloadSha256}`);
