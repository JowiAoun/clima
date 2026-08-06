// SPDX-FileCopyrightText: 2026 Jowi Aoun
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The preferences window. Placement, and a list of tiles.
//
// ============================================================================
// WHERE THE TILE LIST COMES FROM
//
// `clima-widget --list`, run once when this window opens. Not a hard-coded
// list, and that is the whole reason the catalogue is a file the app serves:
// this window ships from extensions.gnome.org and the app from Flathub, so a
// list written here would be a list that is wrong for anybody whose app is
// newer than their extension.
//
// If the app is not installed the list is empty and the window says so, which
// is also the answer to "why is my desktop blank".

import Adw from 'gi://Adw';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import Gtk from 'gi://Gtk';

import {ExtensionPreferences, gettext as _}
    from 'resource:///org/gnome/Shell/Extensions/js/extensions/prefs.js';

const APP_ID = 'io.github.JowiAoun.Clima';

// Asks the installed app what it can draw. Returns [{id, title, summary}].
//
// Synchronous, and that is a deliberate exception to the usual rule: this runs
// once, when a preferences window opens, against a local process that prints a
// dozen lines and exits. An async version would need a spinner and a
// half-populated window, which is more moving parts than the wait is worth.
function catalogue() {
    const dev = GLib.getenv('CLIMA_WIDGET');
    const host = dev || GLib.find_program_in_path('clima-widget');

    let argv;
    if (host) {
        argv = [host, '--list'];
    } else if (GLib.find_program_in_path('flatpak')) {
        argv = ['flatpak', 'run', '--command=clima-widget', APP_ID, '--list'];
    } else {
        return [];
    }

    try {
        const [ok, out] = GLib.spawn_sync(null, argv, null,
                                          GLib.SpawnFlags.SEARCH_PATH, null);
        if (!ok)
            return [];

        const text = new TextDecoder().decode(out);
        return text.split('\n')
            .map(line => line.trimEnd())
            .filter(line => line !== '')
            .map(line => {
                // `--list` prints "id" padded to a column, then the summary.
                const cut = line.indexOf('  ');
                return cut < 0
                    ? {id: line, summary: ''}
                    : {id: line.slice(0, cut).trim(), summary: line.slice(cut).trim()};
            });
    } catch (_e) {
        return [];
    }
}

export default class ClimaPreferences extends ExtensionPreferences {
    fillPreferencesWindow(window) {
        const settings = this.getSettings();
        const page = new Adw.PreferencesPage();
        window.add(page);

        // ---- what to show --------------------------------------------------

        const tiles = new Adw.PreferencesGroup({
            title: _('Tiles'),
            description: _('Which widgets appear on the desktop, and in what order.'),
        });
        page.add(tiles);

        const known = catalogue();

        if (known.length === 0) {
            tiles.add(new Adw.ActionRow({
                title: _('Clima is not installed'),
                subtitle: _('The extension has nothing to show until the app is installed. '
                            + 'Nothing is broken; there is simply no weather to draw.'),
            }));
        } else {
            const enabled = new Set(settings.get_strv('widgets'));

            for (const widget of known) {
                const row = new Adw.SwitchRow({
                    title: widget.id,
                    subtitle: widget.summary,
                    active: enabled.has(widget.id),
                });
                row.connect('notify::active', () => {
                    if (row.active)
                        enabled.add(widget.id);
                    else
                        enabled.delete(widget.id);

                    // Written back in the catalogue's own order rather than in
                    // the order things were switched on, so the layout on the
                    // desktop does not depend on the sequence somebody clicked
                    // in — which is not a thing anybody remembers doing.
                    settings.set_strv('widgets',
                                      known.filter(w => enabled.has(w.id)).map(w => w.id));
                });
                tiles.add(row);
            }
        }

        // ---- where ---------------------------------------------------------

        const placement = new Adw.PreferencesGroup({
            title: _('Placement'),
            description: _('The tiles sit on the primary monitor, below every window.'),
        });
        page.add(placement);

        const anchors = new Gtk.StringList();
        for (const label of [_('Top left'), _('Top right'),
                             _('Bottom left'), _('Bottom right')])
            anchors.append(label);

        const nicks = ['top-left', 'top-right', 'bottom-left', 'bottom-right'];
        const anchorRow = new Adw.ComboRow({
            title: _('Corner'),
            model: anchors,
            selected: Math.max(0, nicks.indexOf(settings.get_string('anchor'))),
        });
        anchorRow.connect('notify::selected', () =>
            settings.set_string('anchor', nicks[anchorRow.selected]));
        placement.add(anchorRow);

        const margin = new Adw.SpinRow({
            title: _('Margin'),
            subtitle: _('Pixels from that corner'),
            adjustment: new Gtk.Adjustment({lower: 0, upper: 400, stepIncrement: 4}),
        });
        settings.bind('margin', margin, 'value', Gio.SettingsBindFlags.DEFAULT);
        placement.add(margin);

        const columns = new Adw.SpinRow({
            title: _('Columns'),
            subtitle: _('How many tiles per row'),
            adjustment: new Gtk.Adjustment({lower: 1, upper: 6, stepIncrement: 1}),
        });
        settings.bind('columns', columns, 'value', Gio.SettingsBindFlags.DEFAULT);
        placement.add(columns);

        // ---- the top bar ---------------------------------------------------

        const panel = new Adw.PreferencesGroup({
            title: _('Top bar'),
            // Said here as well as in the README, because this is where somebody
            // is standing when they wonder why it does not match the app.
            description: _('The indicator is drawn by GNOME Shell, so it uses the '
                           + 'shell’s own theme and typeface rather than Clima’s.'),
        });
        page.add(panel);

        const indicator = new Adw.SwitchRow({
            title: _('Show the temperature in the top bar'),
        });
        settings.bind('show-indicator', indicator, 'active', Gio.SettingsBindFlags.DEFAULT);
        panel.add(indicator);

        const place = new Adw.EntryRow({
            title: _('Place (leave empty for the app’s home place)'),
            text: settings.get_string('place'),
        });
        place.connect('changed', () => settings.set_string('place', place.text));
        panel.add(place);
    }
}
