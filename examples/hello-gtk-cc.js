#!/usr/bin/env node

var
    GNode = require('../lib/'),
    Gtk = GNode.withCamelCase('Gtk'),
    settings,
    win
;

GNode.startLoop();
Gtk.init(null);

settings = Gtk.Settings.getDefault(),
settings.gtkApplicationPreferDarkTheme = true;
settings.gtkThemeName = "Adwaita";

console.log(settings.gtkEnableAccels);

win = new Gtk.Window({
    title: 'node-gtk',
    // __init__ not implemented yet
    // so the following cannot be windowPosition yet
    window_position: Gtk.WindowPosition.CENTER
});

win.connect('show', Gtk.main);
win.connect('destroy', Gtk.mainQuit);

win.setDefaultSize(200, 80);
win.add(new Gtk.Label({label: 'Hello Gtk+'}));

win.showAll();
