//
// # server.js
//   server code to accept and stpre values from a thermostat
//
var http = require('http');
var path = require('path');
var fs   = require('fs');

var express = require('express');
var router = express();
var server = http.createServer(router);

router.use(express.static(path.resolve(__dirname, 'http')));
router.post('/dataLog', function(req, res) {
    var msg = "";
    req.on('data', function(chunk) {
	  msg += chunk;
	});

	req.on('end', function() {
    res.writeHead(200, "OK", {'Content-Type':'text/html'});
		console.log('End POST: ' + msg);
		res.end();

		stream.write(msg + '\n');
	});
});

server.listen(8080, "0.0.0.0", function(){
  var addr = server.address();
  console.log("Chat server listening at", addr.address + ":" + addr.port);
});

var stream = fs.createWriteStream('dataLog.txt');
stream.once('open', function(fd) {
    stream.write("Start of Log\n");
});
