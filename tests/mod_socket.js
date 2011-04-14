#!/usr/bin/natus

socket = require('socket');

var sock = new socket.Socket();
sock.connect('www.google.com', 'http');
sock.write('GET / HTTP/1.0\r\n\r\n');
var line = sock.readLine();
if (line.indexOf('HTTP/1.0') != 0)
  throw "Invalid status line!"
sock.close();
