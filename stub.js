var http = require('http');
var url = require('url');
var log4js = require('log4js');
log4js.configure({
    appenders:[
        {type:'console'}
    ]
});
var logger = log4js.getLogger('webserver');


// Configure our HTTP server to respond with Hello World to all requests.
var server = http.createServer(function (request, response) {
  logger.info(request.url);

  var queryData = url.parse(request.url, true).query;

  //response.setHeader('transfer-encoding', '');
  response.useChunkedEncodingByDefault = false;

  if (queryData.token) {
    logger.info(queryData.token);
    response.write('{"access":1, "error":"blah"}');
    response.end();
  } else {
      response.write('{"access":0, "error":"missing token"}');
      response.end();
  }
});

server.listen(9000);
console.log('Server is running on port 9000');
