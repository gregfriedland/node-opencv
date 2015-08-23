var cv = require('opencv');

var camera = new cv.VideoCapture(0);

var startTime = new Date().getTime();
var count = 0;

setInterval(function() {
  camera.read(function(err, im, didRead) {
    if (err) throw err;
    if (didRead == false) {
      // console.log("Skipped read.");
    } else {
      count++;
      var currTime = new Date().getTime();
      if (currTime - startTime > 1000) {
        console.log("fps: " + (count * 1000 / (currTime - startTime)));
        startTime = currTime;
        count = 0;
      }
    }
  });
}, 20);