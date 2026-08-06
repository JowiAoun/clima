// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The half of scripts/check-extension.sh that has to run inside gjs.
//
//   gjs -m scripts/check-extension.js <extension directory>
//
// It reads extension.js as TEXT rather than importing it, because importing it
// needs gnome-shell's own resources. That is a real limitation and it shapes
// what can be asserted: the D-Bus XML and the method names are extracted with
// expressions anchored to the exact spellings the file uses, so a rewrite that
// moves them will make this check fail loudly rather than pass vacuously.

import GLib from 'gi://GLib';
import Gio from 'gi://Gio';

const [dir] = ARGV;
if (!dir) {
    printerr('usage: check-extension.js <extension directory>');
    imports.system.exit(2);
}

let failures = 0;

function check(name, ok, detail) {
    if (ok) {
        print(`${name}: ok`);
    } else {
        printerr(`${name}: FAILED — ${detail}`);
        failures += 1;
    }
}

function read(path) {
    const [ok, bytes] = GLib.file_get_contents(path);
    if (!ok)
        throw new Error(`could not read ${path}`);
    return new TextDecoder().decode(bytes);
}

const source = read(`${dir}/extension.js`);

// ---- 2: the D-Bus introspection XML -----------------------------------------

const xmlMatch = source.match(/const DAEMON_XML = `([\s\S]*?)`;/);
check('dbus-xml: found in extension.js', xmlMatch !== null,
      'DAEMON_XML is not a template literal any more; update this check');

if (xmlMatch) {
    // The XML interpolates ${DAEMON_IFACE}. Substituted here rather than
    // stripped, so the parsed interface carries the real name and the next
    // assertion is about the name the extension will actually talk to.
    const ifaceMatch = source.match(/const DAEMON_IFACE = '([^']+)'/);
    const xml = xmlMatch[1].replace('${DAEMON_IFACE}', ifaceMatch ? ifaceMatch[1] : 'x');

    let node = null;
    try {
        node = Gio.DBusNodeInfo.new_for_xml(xml);
    } catch (e) {
        check('dbus-xml: parses', false, e.message);
    }

    if (node) {
        check('dbus-xml: parses', true);

        const iface = node.interfaces[0];
        check('dbus-xml: names the daemon interface',
              iface && iface.name === 'io.github.JowiAoun.Clima.Daemon1',
              `got ${iface ? iface.name : '(none)'}`);

        // Every method the extension calls, against what the XML declares. The
        // GJS proxy wrapper generates <Name>Remote() from the XML, so a method
        // the extension calls but the XML omits is a TypeError at click time —
        // which for `RequestRefreshRemote` means a menu item that throws in the
        // journal and does nothing on screen.
        const declared = new Set((iface ? iface.methods : []).map(m => m.name));
        for (const method of ['SchemaVersion', 'Subscribe', 'Unsubscribe', 'RequestRefresh']) {
            check(`dbus-xml: declares ${method}`, declared.has(method),
                  'the extension calls it and the XML does not describe it');
            check(`extension: calls ${method}Remote`,
                  source.includes(`${method}Remote(`),
                  'declared in the XML and never used — dead interface surface');
        }

        const signals = new Set((iface ? iface.signals : []).map(s => s.name));
        check('dbus-xml: declares SnapshotChanged', signals.has('SnapshotChanged'),
              'the indicator connects to it');
    }
}

// ---- 3: the mutter API the adoption rests on --------------------------------
//
// The check that would have saved an afternoon. The spike this extension grew
// out of called `query_window_belongs_to`, which is DING's own wrapper name and
// not a method on MetaWaylandClient at all — a TypeError an hour into a nested
// shell run. Every name below is one this extension calls.

let Meta = null;
try {
    Meta = (await import('gi://Meta')).default;
} catch (_e) {
    print('waylandclient: SKIPPED (no Meta typelib on this machine)');
}

if (Meta) {
    const methods = Object.getOwnPropertyNames(Meta.WaylandClient.prototype)
        .filter(n => n !== 'constructor');

    print(`waylandclient: mutter exposes ${methods.sort().join(' ')}`);

    for (const method of ['spawnv', 'owns_window', 'make_dock', 'hide_from_window_list']) {
        check(`waylandclient: ${method}`, methods.includes(method),
              'the extension calls it and this mutter does not have it');

        // `_client.<method>(` and not `<method>(`. The looser search passed on
        // the word appearing in a comment — and the comments in extension.js
        // name every one of these methods while explaining them, so renaming a
        // call site left the check green. Anchoring on the receiver is what
        // makes this an assertion about code.
        check(`extension: calls ${method}`, source.includes(`_client.${method}(`),
              'checked for here and not called — either this list has gone stale '
              + 'or a call site was renamed');
    }

    check('meta: is_wayland_compositor', typeof Meta.is_wayland_compositor === 'function',
          'the extension gates itself on it');
}

// ---- 4: metadata -------------------------------------------------------------

let metadata = null;
try {
    metadata = JSON.parse(read(`${dir}/metadata.json`));
    check('metadata: parses', true);
} catch (e) {
    check('metadata: parses', false, e.message);
}

if (metadata) {
    check('metadata: uuid matches the directory name',
          `${dir}`.endsWith(metadata.uuid),
          `uuid is ${metadata.uuid}; gnome-shell will not load a mismatched directory`);

    check('metadata: names its settings schema',
          metadata['settings-schema'] === 'org.gnome.shell.extensions.clima',
          'getSettings() resolves through this');

    check('metadata: declares shell versions',
          Array.isArray(metadata['shell-version']) && metadata['shell-version'].length > 0,
          'extensions.gnome.org rejects an upload without one');

    // 45 is where ESM extensions arrived. A file that says `export default
    // class extends Extension` cannot load on anything older, and declaring it
    // would be an upload that fails for the user rather than for us.
    const oldest = Math.min(...metadata['shell-version'].map(v => parseInt(v, 10)));
    check('metadata: does not claim a pre-ESM shell', oldest >= 45,
          `declares ${oldest}; this extension is an ES module and needs 45 or newer`);
}

if (failures > 0) {
    printerr(`check-extension.js: ${failures} check(s) failed`);
    imports.system.exit(1);
}
