// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Clima's GNOME Shell extension. It draws no weather.
//
// ============================================================================
// WHAT THIS DOES, AND WHY IT CANNOT DO THE OBVIOUS THING
//
// An extension is GJS running inside gnome-shell's own process. It cannot host
// a Qt Quick surface, extensions.gnome.org forbids shipping binaries, and
// mutter still does not implement wlr-layer-shell — so there is no protocol by
// which an outside process can ask to be a desktop layer either.
//
// So a Clima widget is *our own Qt process*, and this file's whole job is to
// launch it and pin its window to the desktop. That is the DING pattern, and
// the mechanism was measured on a real shell before any of it was written:
// docs/widgets.md in the Clima repository has the numbers.
//
// Everything a user looks at is QML, which is why the tiles share components
// with the app instead of being a second implementation of the same weather.
//
// ============================================================================
// THE PANEL INDICATOR IS NOT THE APP, AND WILL NEVER LOOK LIKE IT
//
// The temperature in the top bar and the menu behind it are drawn in St with
// the shell's own theme. They cannot use Clima's typeface, its colour tokens or
// its charts, because none of that exists inside gnome-shell. This is stated
// here and in the README so that nobody files it as a bug: a St popup that
// looked like the app would mean reimplementing the design system in CSS, and
// it would drift the first time either side changed.
//
// ============================================================================
// WAYLAND ONLY, DELIBERATELY
//
// Meta.WaylandClient is what establishes that a window is ours: the shell makes
// a socketpair, keeps one end, and hands the child the other as WAYLAND_SOCKET.
// owns_window() then answers a question about *that* wl_client. On X11 there is
// no such object, the adoption cannot be verified, and a window that is merely
// lowered is one the user can raise by clicking it.
//
// GNOME has defaulted to Wayland since 3.34 and Ubuntu since 21.04. Rather than
// ship an X11 path nobody measured, the extension says what it needs and stops.

import Clutter from 'gi://Clutter';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import Meta from 'gi://Meta';
import St from 'gi://St';

import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import * as PanelMenu from 'resource:///org/gnome/shell/ui/panelMenu.js';
import * as PopupMenu from 'resource:///org/gnome/shell/ui/popupMenu.js';
import {Extension, gettext as _} from 'resource:///org/gnome/shell/extensions/extension.js';

// ---- the daemon ------------------------------------------------------------
//
// Kept in step with daemon/daemonconfig.h.in by hand, because this file ships
// from extensions.gnome.org and the app ships from Flathub — there is no build
// that sees both. The names are part of the interface contract and change only
// with the schema version below.

const DAEMON_NAME = 'io.github.JowiAoun.Clima.Daemon';
const DAEMON_PATH = '/io/github/JowiAoun/Clima/Daemon';
const DAEMON_IFACE = 'io.github.JowiAoun.Clima.Daemon1';

const APP_ID = 'io.github.JowiAoun.Clima';

// The JSON shape this file knows how to read. Deliberately a separate number
// from the daemon's: the two ship on different clocks and will routinely
// disagree by a version, and an indicator that draws a guess from a shape it
// does not understand is worse than one that says so.
const UNDERSTOOD_SCHEMA = 1;

const DAEMON_XML = `
<node>
  <interface name="${DAEMON_IFACE}">
    <method name="SchemaVersion">
      <arg type="i" direction="out"/>
    </method>
    <method name="Subscribe">
      <arg type="s" direction="in"/>
      <arg type="as" direction="in"/>
      <arg type="i" direction="in"/>
      <arg type="i" direction="in"/>
      <arg type="s" direction="out"/>
    </method>
    <method name="Unsubscribe">
      <arg type="s" direction="in"/>
      <arg type="b" direction="out"/>
    </method>
    <method name="RequestRefresh">
      <arg type="s" direction="in"/>
    </method>
    <signal name="SnapshotChanged">
      <arg type="s"/>
      <arg type="s"/>
    </signal>
  </interface>
</node>`;

const DaemonProxy = Gio.DBusProxy.makeProxyWrapper(DAEMON_XML);

// What the top bar needs and nothing else. The field mask is the whole point of
// the wire format: an indicator that shows one number is not sent 408 hourly
// points.
const INDICATOR_FIELDS = [
    'place',
    'current.temperature',
    'current.weatherCode',
    'current.isDay',
];

// ---- respawn ---------------------------------------------------------------
//
// A crashed widget host comes back, and a widget host that cannot start does
// not spin. Doubling from two seconds to a minute, reset the moment a window is
// actually adopted — so a genuine crash loop settles into one attempt a minute
// in the journal rather than several thousand.
const RESPAWN_FIRST_MS = 2000;
const RESPAWN_MAX_MS = 60000;

function log_(message) {
    console.log(`clima: ${message}`);
}

// ============================================================================
// The widget host
// ============================================================================

class WidgetHost {
    constructor(settings) {
        this._settings = settings;
        this._client = null;
        this._proc = null;
        this._window = null;
        this._mapId = 0;
        this._monitorsId = 0;
        this._respawnId = 0;
        this._positionId = 0;
        this._respawnDelay = RESPAWN_FIRST_MS;
        this._stopping = false;
    }

    // The command that starts the tiles, or null when Clima is not installed.
    //
    // A host binary wins over the Flatpak: a developer with a build in their
    // PATH is testing that build. Both work — `flatpak run` execs bwrap, and
    // the inherited socket fd and WAYLAND_SOCKET both survive it, which was the
    // single riskiest assumption in this design and is the one docs/widgets.md
    // spends the most space on.
    _command() {
        const ids = this._settings.get_strv('widgets');
        const place = this._settings.get_string('place');
        const columns = this._settings.get_int('columns');

        const args = [];
        for (const id of ids)
            args.push('--widget', id);
        if (place !== '')
            args.push('--place', place);
        args.push('--columns', `${columns}`);

        const dev = GLib.getenv('CLIMA_WIDGET');
        if (dev)
            return [dev, ...args];

        const host = GLib.find_program_in_path('clima-widget');
        if (host)
            return [host, ...args];

        const flatpak = GLib.find_program_in_path('flatpak');
        if (flatpak && this._flatpakInstalled())
            return [flatpak, 'run', `--command=clima-widget`, APP_ID, ...args];

        return null;
    }

    _flatpakInstalled() {
        const exports = [
            `${GLib.get_home_dir()}/.local/share/flatpak/exports/bin/${APP_ID}`,
            `/var/lib/flatpak/exports/bin/${APP_ID}`,
        ];
        return exports.some(p => GLib.file_test(p, GLib.FileTest.EXISTS));
    }

    start() {
        this._stopping = false;

        const argv = this._command();
        if (argv === null) {
            // Once, quietly, and then never again. A user who has the extension
            // enabled and the app uninstalled is a normal state — they removed
            // the Flatpak — and it must not produce a notification storm or a
            // stack trace in their journal.
            log_('the Clima app is not installed, so there are no widgets to show.');
            return false;
        }

        // NONE, so the child inherits gnome-shell's own stdout and stderr and
        // whatever it says lands in the journal beside the shell's own lines.
        // Silencing it would make a widget host that dies on startup
        // indistinguishable from one that started and drew nothing, which is
        // the exact failure this is most likely to have.
        const launcher = new Gio.SubprocessLauncher({
            flags: Gio.SubprocessFlags.NONE,
        });

        // Two signatures exist in the wild. mutter 14 (GNOME 46) takes the
        // context; older ones do not. Measured on both by the probe this file
        // grew out of.
        try {
            this._client = Meta.WaylandClient.new(launcher);
        } catch (_e) {
            this._client = Meta.WaylandClient.new(global.context, launcher);
        }

        // connect_after, so mutter has finished its own placement before we
        // move the window. Connecting to `map` itself gets a window whose frame
        // rect is not yet what it will be.
        this._mapId = global.window_manager.connect_after('map', (_wm, actor) => {
            this._onMap(actor.get_meta_window());
        });

        try {
            this._proc = this._client.spawnv(global.display, argv);
        } catch (e) {
            log_(`could not launch the widget host: ${e.message}`);
            this._teardownClient();
            this._scheduleRespawn();
            return false;
        }

        this._proc.wait_async(null, () => {
            if (this._stopping)
                return;
            log_('the widget host exited; restarting.');
            this._window = null;
            this._teardownClient();
            this._scheduleRespawn();
        });

        this._monitorsId = Main.layoutManager.connect('monitors-changed', () => this._place());

        return true;
    }

    _onMap(w) {
        if (this._client === null || this._window !== null)
            return;

        // owns_window() is the question, and it is a question about the
        // wl_client rather than about anything visible. Matching on a window
        // title or a sandboxed app id would be guessing — and the sandbox id
        // comes back null for a client that connected on an inherited fd, which
        // is measured in docs/widgets.md and is a reasonable-looking idea that
        // does not work.
        let ours = false;
        try {
            ours = this._client.owns_window(w);
        } catch (_e) {
            ours = false;
        }
        if (!ours)
            return;

        this._window = w;
        this._respawnDelay = RESPAWN_FIRST_MS;

        // make_dock() rather than DING's title-flag hack, which predates the
        // API: a DOCK is on every workspace and out of the overview by
        // construction, which is most of what parsing `@!B` out of a window
        // title was emulating.
        try {
            this._client.make_dock(w);
        } catch (e) {
            log_(`make_dock failed: ${e.message}`);
        }

        // Out of alt-tab, and still composited. Hidden from the window list,
        // not from the user.
        try {
            this._client.hide_from_window_list(w);
        } catch (e) {
            log_(`hide_from_window_list failed: ${e.message}`);
        }

        w.lower();
        this._place();

        this._positionId = w.connect('position-changed', () => this._remember());
    }

    // Where the tiles sit. An anchor plus an offset rather than absolute
    // coordinates, so that unplugging a monitor does not leave the tiles off
    // the edge of the one that is left.
    reposition() {
        this._place();
    }

    _place() {
        const w = this._window;
        if (w === null)
            return;

        const monitor = Main.layoutManager.primaryMonitor;
        if (!monitor)
            return;

        const rect = w.get_frame_rect();
        const margin = this._settings.get_int('margin');
        const anchor = this._settings.get_string('anchor');

        const right = anchor.endsWith('-right');
        const bottom = anchor.startsWith('bottom-');

        const x = right
            ? monitor.x + monitor.width - rect.width - margin
            : monitor.x + margin;
        const y = bottom
            ? monitor.y + monitor.height - rect.height - margin
            : monitor.y + margin;

        w.move_frame(false, Math.round(x), Math.round(y));
        w.lower();
    }

    // The user dragged them. There is nothing to drag a DOCK by today — it has
    // no titlebar — but the handler is here because the geometry has to survive
    // a shell restart either way, and reading it back off the window is the
    // only source of truth for it.
    _remember() {
        const w = this._window;
        if (w === null)
            return;
        const rect = w.get_frame_rect();
        this._settings.set_int('last-width', rect.width);
        this._settings.set_int('last-height', rect.height);
    }

    _scheduleRespawn() {
        if (this._stopping || this._respawnId !== 0)
            return;

        const delay = this._respawnDelay;
        this._respawnDelay = Math.min(this._respawnDelay * 2, RESPAWN_MAX_MS);

        this._respawnId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, delay, () => {
            this._respawnId = 0;
            if (!this._stopping)
                this.start();
            return GLib.SOURCE_REMOVE;
        });
    }

    // Called when settings change: the tile list, the place, the column count.
    // A restart rather than a message, because the host reads all three at
    // startup and there is no interface for changing them afterwards — which is
    // the right trade for something a user touches twice a year.
    restart() {
        this.stop();
        this.start();
    }

    _teardownClient() {
        if (this._mapId !== 0) {
            global.window_manager.disconnect(this._mapId);
            this._mapId = 0;
        }
        if (this._monitorsId !== 0) {
            Main.layoutManager.disconnect(this._monitorsId);
            this._monitorsId = 0;
        }
        this._client = null;
        this._proc = null;
    }

    // Everything, and it has to be everything: the shell disables extensions on
    // the lock screen, and a widget host left running would draw the weather
    // over a locked session.
    stop() {
        this._stopping = true;

        if (this._respawnId !== 0) {
            GLib.source_remove(this._respawnId);
            this._respawnId = 0;
        }
        if (this._window !== null && this._positionId) {
            try {
                this._window.disconnect(this._positionId);
            } catch (_e) {
                // The window is already gone; nothing to disconnect from.
            }
            this._positionId = 0;
        }
        if (this._proc !== null) {
            try {
                this._proc.force_exit();
            } catch (_e) {
                // Already dead.
            }
        }

        this._window = null;
        this._teardownClient();
        this._respawnDelay = RESPAWN_FIRST_MS;
    }
}

// ============================================================================
// The panel indicator
// ============================================================================

const ClimaIndicator = GObject.registerClass(
class ClimaIndicator extends PanelMenu.Button {
    _init(settings) {
        super._init(0.5, 'Clima');

        this._settings = settings;
        this._proxy = null;
        this._token = '';
        this._signalId = 0;
        this._watchId = 0;

        this._label = new St.Label({
            text: '—',
            yAlign: Clutter.ActorAlign.CENTER,
            styleClass: 'clima-indicator-label',
        });
        this.add_child(this._label);

        this._place = new PopupMenu.PopupMenuItem(_('Clima'), {reactive: false});
        this.menu.addMenuItem(this._place);

        this._status = new PopupMenu.PopupMenuItem('', {reactive: false});
        this.menu.addMenuItem(this._status);

        this.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());

        const refresh = new PopupMenu.PopupMenuItem(_('Refresh now'));
        refresh.connect('activate', () => this._refresh());
        this.menu.addMenuItem(refresh);

        const open = new PopupMenu.PopupMenuItem(_('Open Clima'));
        open.connect('activate', () => this._openApp());
        this.menu.addMenuItem(open);

        this._connect();
    }

    // Watched rather than called once. The daemon is an ordinary process that
    // can be upgraded, killed or started after the shell — and when it goes
    // away the last reading stays on screen rather than blanking, because a
    // stale number with nothing claiming it is current is more use than a dash.
    _connect() {
        this._watchId = Gio.bus_watch_name(
            Gio.BusType.SESSION, DAEMON_NAME, Gio.BusNameWatcherFlags.NONE,
            () => this._onAppeared(),
            () => this._onVanished());
    }

    _onAppeared() {
        try {
            this._proxy = new DaemonProxy(Gio.DBus.session, DAEMON_NAME, DAEMON_PATH);
        } catch (e) {
            log_(`could not reach the daemon: ${e.message}`);
            return;
        }

        this._proxy.SchemaVersionRemote(([version], error) => {
            if (error) {
                log_(`SchemaVersion failed: ${error.message}`);
                return;
            }
            if (version !== UNDERSTOOD_SCHEMA) {
                this._status.label.text =
                    _('This extension and the Clima app are different versions.');
                this._label.text = '⚠';
                return;
            }
            this._subscribe();
        });
    }

    _subscribe() {
        const place = this._settings.get_string('place');

        this._proxy.SubscribeRemote(place, INDICATOR_FIELDS, 0, 0, ([token], error) => {
            if (error || !token) {
                log_(`Subscribe failed: ${error ? error.message : 'no token'}`);
                return;
            }
            this._token = token;

            // Filtered on the token here rather than by a bus match rule,
            // because GJS's proxy wrapper does not expose arg0 matching. It
            // costs one wakeup per other subscriber, which for a single
            // indicator is nothing; the widget host, which has one subscription
            // per tile, does add the match rule.
            this._signalId = this._proxy.connectSignal(
                'SnapshotChanged', (_p, _sender, [token_, json]) => {
                    if (token_ === this._token)
                        this._render(json);
                });
        });
    }

    _onVanished() {
        this._token = '';
        this._proxy = null;
        this._signalId = 0;
        this._status.label.text = _('Clima is not running.');
    }

    _render(json) {
        let snapshot;
        try {
            snapshot = JSON.parse(json);
        } catch (e) {
            log_(`a snapshot did not parse: ${e.message}`);
            return;
        }

        const current = snapshot.current || {};

        // null is not zero, and it is not "0 °C" either. The wire keeps absent
        // distinguishable all the way here and the indicator has to respect it:
        // showing 0° for a place with no reading is a temperature nobody
        // measured, in the top bar, all day.
        this._label.text = typeof current.temperature === 'number'
            ? `${Math.round(current.temperature)}°`
            : '—';

        const place = snapshot.place || {};
        this._place.label.text = place.name || _('Clima');

        if (snapshot.state === 'live')
            this._status.label.text = _('Up to date');
        else if (snapshot.state === 'cached')
            this._status.label.text = _('Showing the last reading');
        else
            this._status.label.text = _('No reading yet');
    }

    _refresh() {
        if (this._proxy === null)
            return;
        this._proxy.RequestRefreshRemote(this._settings.get_string('place'));
    }

    _openApp() {
        const app = Gio.DesktopAppInfo.new(`${APP_ID}.desktop`);
        if (app === null) {
            log_('no desktop entry for the Clima app.');
            return;
        }
        app.launch([], global.create_app_launch_context(0, -1));
    }

    destroy() {
        if (this._proxy !== null && this._token !== '') {
            try {
                this._proxy.UnsubscribeRemote(this._token, () => {});
            } catch (_e) {
                // The daemon went away first, which is fine: it drops the
                // subscription when the connection closes.
            }
        }
        if (this._watchId !== 0) {
            Gio.bus_unwatch_name(this._watchId);
            this._watchId = 0;
        }
        this._proxy = null;
        super.destroy();
    }
});

// ============================================================================

export default class ClimaExtension extends Extension {
    enable() {
        this._settings = this.getSettings();

        if (!Meta.is_wayland_compositor()) {
            // Not an error dialog and not a notification. The shell is working;
            // this extension simply cannot do its job on this session type, and
            // the honest place to say so is the journal and the preferences
            // window.
            log_('this extension needs a Wayland session. See the README.');
            return;
        }

        this._host = new WidgetHost(this._settings);
        this._host.start();

        if (this._settings.get_boolean('show-indicator')) {
            this._indicator = new ClimaIndicator(this._settings);
            Main.panel.addToStatusArea(this.uuid, this._indicator);
        }

        // The tile list, the place and the column count are read by the host at
        // startup, so a change to any of them is a restart of it.
        this._changedIds = ['widgets', 'place', 'columns'].map(key =>
            this._settings.connect(`changed::${key}`, () => this._host.restart()));

        this._changedIds.push(
            this._settings.connect('changed::anchor', () => this._host.reposition()),
            this._settings.connect('changed::margin', () => this._host.reposition()));
    }

    disable() {
        for (const id of this._changedIds ?? [])
            this._settings.disconnect(id);
        this._changedIds = [];

        this._indicator?.destroy();
        this._indicator = null;

        this._host?.stop();
        this._host = null;

        this._settings = null;
    }
}
