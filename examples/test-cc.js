
const GNode = require('../lib/');
GNode.startLoop();

const GLib = GNode.withCamelCase("GLib");
console.log(GLib.asciiStrup("foo", -1));

const GUdev = GNode.withCamelCase("GUdev");
var client = new GUdev.Client();
var obj = client.queryByDeviceFile("/dev/dri/card0");
console.log(obj.getName());

console.log(GLib.test_override());

const Gtk = GNode.withCamelCase("Gtk");
Gtk.init(null);

var w = new Gtk.Window();
var b = new Gtk.Button({ label: "Hi!" });
b.connect('clicked', function() { console.log("BB"); });
w.add(b);
w.showAll();

Gtk.main();
