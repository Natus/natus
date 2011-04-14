#!/usr/bin/natus

socket = require('socket');

alert("1");
var sock = new socket.Socket();
alert("2");
sock.connect('www.google.com', 'http');
alert("3");
sock.write('GET / HTTP/1.0\r\n\r\n');
alert("4");
var line = sock.readLine();
alert("5");
if (line != 'HTTP/1.0 200 OK')
  throw "Invalid status line!"
alert("6");
sock.close();
