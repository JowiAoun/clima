// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A test fixture, not a product. `.invalid` in the UUID is RFC 2606 and says
// so: this is never published to extensions.gnome.org.
//
// ============================================================================
// WHAT THIS ANSWERS
//
// The Ubuntu widget story rests on one mechanism, and the mechanism is not
// obvious enough to take on trust. GNOME Shell cannot host a QML surface — an
// extension is GJS running inside gnome-shell's own process, and
// extensions.gnome.org forbids shipping binaries — so a Clima widget cannot be
// drawn *by* the extension. It has to be our own Qt process, whose window the
// extension then adopts and pins to the desktop. That is the DING pattern, and
// `Meta.WaylandClient` is the whole of it.
//
// The part worth testing is that the client identity is established by an
// inherited socket fd. `meta_wayland_client_spawnv()` creates a socketpair,
// keeps the server end, and hands the child the other end as WAYLAND_SOCKET.
// Everything downstream — owns_window(), hide_from_window_list(), make_dock()
// — is a question about *that* wl_client.
//
// Two consequences follow, and this probe exists to prove or disprove them on
// a real machine rather than reason about them:
//
//   1. If the app is installed as a Flatpak, the spawn goes through
//      `flatpak run`, which execs bwrap. If bwrap closed the inherited fd or
//      filtered the env var, the app would connect to the compositor the
//      ordinary way, owns_window() would be false, and the window could never
//      be adopted. Measured: it survives. See docs/widgets.md.
//
//   2. D-Bus activation cannot be used to start the widget host. A
//      bus-activated process is spawned by dbus-daemon, not by the shell, so
//      no MetaWaylandClient ever owns it. This is worth stating because it is
//      the mechanism one reaches for first.
//
// ============================================================================
// READING THE OUTPUT
//
// The last line beginning `PROBE-VERDICT ` is a JSON object; scripts/shell-
// probe.sh parses it and asserts. Everything else is narration, including the
// target's own stdout, which is the only place a Qt app that dies on startup
// says why.

import Meta from 'gi://Meta';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';

const REPORT = GLib.getenv('CLIMA_PROBE_REPORT');
const TARGET = GLib.getenv('CLIMA_PROBE_ARGV');
const MODE = GLib.getenv('CLIMA_PROBE_MODE') || 'dock';

// argv arrives joined by the unit separator so a path may contain spaces.
const SEP = '\x1f';

// Generous, and it has to be. Under a private session bus there is no portal,
// so a sandboxed Qt app spends ~25 s timing out on org.freedesktop.portal.
// Desktop before it paints. A tighter deadline measures the portal, not us.
const DEADLINE_SECONDS = 90;

// Long enough for mutter to have restacked and re-typed the window before we
// read any of it back. Asking immediately after make_dock() reads the old type.
const SETTLE_MS = 2500;

export default class ClimaWindowProbe extends Extension {
    enable() {
        this._events = [];
        this._finished = false;

        this._say(`probe start, mode=${MODE}`);
        this._say(`argv: ${TARGET ? TARGET.split(SEP).join(' ') : '(none)'}`);
        this._say(`monitors: ${Main.layoutManager.monitors.length}`);
        this._say(`WaylandClient: ${Object.getOwnPropertyNames(Meta.WaylandClient.prototype)
            .filter(n => n !== 'constructor').sort().join(' ')}`);

        const launcher = new Gio.SubprocessLauncher({
            flags: Gio.SubprocessFlags.STDOUT_PIPE | Gio.SubprocessFlags.STDERR_MERGE,
        });

        // Two signatures exist in the wild. mutter 14 (GNOME 46) takes the
        // context; older ones do not.
        try {
            this._client = Meta.WaylandClient.new(launcher);
        } catch (e) {
            this._client = Meta.WaylandClient.new(global.context, launcher);
        }

        this._mapId = global.window_manager.connect_after('map', (wm, actor) => {
            this._onMap(actor.get_meta_window());
        });

        try {
            this._proc = this._client.spawnv(global.display, TARGET.split(SEP));
        } catch (e) {
            this._say(`spawnv threw: ${e.message}`);
            this._fail(`spawnv threw: ${e.message}`);
            return;
        }

        this._pump();

        this._deadline = GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT, DEADLINE_SECONDS, () => {
            this._deadline = 0;
            this._fail('deadline passed with no window we own');
            return GLib.SOURCE_REMOVE;
        });
    }

    _onMap(w) {
        let owns;
        try {
            owns = this._client.owns_window(w);
        } catch (e) {
            owns = `threw: ${e.message}`;
        }

        // Recorded for every window, ours or not. A run in which *no* window
        // is ours reads very differently from one with no windows at all, and
        // the difference is the first thing you want when this fails.
        this._say(`map: ${JSON.stringify({
            wm_class: w.get_wm_class(),
            title: w.get_title(),
            owns_window: owns,
        })}`);

        if (owns === true) {
            this._adopt(w);
        }
    }

    _adopt(w) {
        const v = {owns_window: true, mode: MODE};

        // make_dock/make_desktop are mutter's own re-typing calls. DING does
        // not use them — it encodes flags in the window title and parses them
        // back out — because it predates them. They are what a new extension
        // should use: a DOCK is on every workspace and out of the overview by
        // construction, which is most of what the title hack was emulating.
        if (MODE === 'dock' || MODE === 'desktop') {
            const fn = MODE === 'dock' ? 'make_dock' : 'make_desktop';
            try {
                this._client[fn](w);
                v[fn] = true;
            } catch (e) {
                v[fn] = `threw: ${e.message}`;
            }
        }

        try {
            this._client.hide_from_window_list(w);
            v.hide_from_window_list = true;
        } catch (e) {
            v.hide_from_window_list = `threw: ${e.message}`;
        }

        try {
            w.lower();
            v.lower = true;
        } catch (e) {
            v.lower = `threw: ${e.message}`;
        }

        GLib.timeout_add(GLib.PRIORITY_DEFAULT, SETTLE_MS, () => {
            v.window_type = w.get_window_type();

            // get_tab_list is what alt-tab reads. If hide_from_window_list did
            // anything at all, our window is not in it.
            const ws = global.workspace_manager.get_active_workspace();
            v.in_tab_list = global.display
                .get_tab_list(Meta.TabList.NORMAL_ALL, ws)
                .some(t => t === w);

            // ...but it must still be composited, or we have hidden it from
            // the user rather than from the window list.
            v.in_window_actors = global.get_window_actors()
                .some(a => a.get_meta_window() === w);

            v.on_all_workspaces = w.is_on_all_workspaces();

            const r = w.get_frame_rect();
            v.rect = `${r.width}x${r.height}+${r.x}+${r.y}`;

            this._verdict(v);
            return GLib.SOURCE_REMOVE;
        });
    }

    // The target's stdout, into our log. Without this a Qt app that fails to
    // start is indistinguishable from one that started and drew nothing.
    _pump() {
        const stream = Gio.DataInputStream.new(this._proc.get_stdout_pipe());
        const read = () => {
            stream.read_line_async(GLib.PRIORITY_LOW, null, (o, res) => {
                let line = null;
                try {
                    [line] = o.read_line_finish_utf8(res);
                } catch (e) {
                    return;   // stream closed
                }
                if (line !== null) {
                    this._say(`target: ${line}`);
                    read();
                }
            });
        };
        read();
    }

    _say(msg) {
        const line = `PROBE ${msg}`;
        console.log(line);
        if (!REPORT) {
            return;
        }
        try {
            const os = Gio.File.new_for_path(REPORT).append_to(Gio.FileCreateFlags.NONE, null);
            os.write_all(`${line}\n`, null);
            os.close(null);
        } catch (e) { /* best effort; the journal still has it */ }
    }

    _fail(reason) {
        this._verdict({owns_window: false, failure: reason});
    }

    _verdict(v) {
        if (this._finished) {
            return;
        }
        this._finished = true;
        this._say(`VERDICT ${JSON.stringify(v)}`);
        try {
            this._proc && this._proc.force_exit();
        } catch (e) { /* already gone */ }

        // Let the write land before the compositor goes away under us.
        GLib.timeout_add(GLib.PRIORITY_DEFAULT, 300, () => {
            Meta.quit(Meta.ExitCode.SUCCESS);
            return GLib.SOURCE_REMOVE;
        });
    }

    disable() {
        if (this._mapId) {
            global.window_manager.disconnect(this._mapId);
            this._mapId = 0;
        }
        if (this._deadline) {
            GLib.source_remove(this._deadline);
            this._deadline = 0;
        }
    }
}
