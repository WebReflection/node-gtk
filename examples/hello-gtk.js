#!/usr/bin/env node

var
  GNode = require('../lib/'),
  Gtk = GNode.importNS('Gtk'),
  win
;

GNode.startLoop();
Gtk.init(null);

console.log(Gtk.Settings.getDefault().gtkEnableAccels);

win = new Gtk.Window({
  title: 'node-gtk',
  window_position: Gtk.WindowPosition.CENTER
});

win.connect('show', Gtk.main);
win.connect('destroy', Gtk.mainQuit);

win.setDefaultSize(200, 80);
win.add(new Gtk.Label({label: 'Hello Gtk+'}));

win.showAll();
