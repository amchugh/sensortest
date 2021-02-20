const String PAGEHTML = R""""(
<DOCTYPE html>
<html>
  <head>
  <script src="https://cdnjs.cloudflare.com/ajax/libs/paho-mqtt/1.0.1/mqttws31.min.js" type="text/javascript"></script>
  <script>
var mqtt;
var graph;
var line;
var lineshadow;
var reconnectTimeout = 2000;
var host = "192.168.50.41";
var port = 9001;

const MAXPOINTS=100;

const GRAPHWIDTH = 400.0;
const GRAPHHEIGHT = 400.0;

const SCALEX = GRAPHWIDTH/(MAXPOINTS-1);

const RECTHRESHOLD = 0.3;

function grapher() {
  this.scaley = 1;
  this.record = 1;
  this.data = [];
  this.dirty = false;
  this.getFormula = function() {
    let ld = "";
    /* Move to the starting left point */
    let startX = (GRAPHWIDTH - (this.data.length - 1) * SCALEX);
    ld += "M " + startX + " " + (GRAPHHEIGHT - this.data[0] * this.scaley);
    /* Add lines to the rest of the points */
    for (let i = 1; i < this.data.length; i++) {
      ld += " L " + (startX + i * SCALEX) + " " + (GRAPHHEIGHT - this.data[i] * this.scaley);
    }
    return {
      "std": ld,
      "shadow": ld + " L " + GRAPHWIDTH + " " + GRAPHHEIGHT + " L " + startX + " " + GRAPHHEIGHT + " Z",
    };
  };
  this.addData = function(x) {
    this.data.push(x);
    if (x > this.record) { this.record = x; this.calcScale(); }
    else if (x <= this.record * RECTHRESHOLD) { this.record = x / RECTHRESHOLD; this.calcScale(); }
    if (this.data.length > MAXPOINTS) this.data.shift();
  };
  this.calcScale = function() {
    this.scaley = GRAPHHEIGHT/parseFloat(this.record)*0.8;
    if (!isFinite(this.scaley)) this.scaley = 200.0;
    this.dirty = true;
  };
  this.isScaleDirty = function() {
    if (this.dirty) {
      this.dirty = false;
      return true;
    }
    return false;
  };
}

const NUMGRAPHS=3;
const myGraphs = [
  new grapher(),
  new grapher(),
  new grapher(),
];

var selected = 0;

function onMessage(msg) {
  if (msg.destinationName == "aq") {
    const d = msg.payloadString.split(",");
    console.debug(d);
    myGraphs[0].addData(parseInt(d[0]));
    myGraphs[1].addData(parseInt(d[1]));
    myGraphs[2].addData(parseInt(d[2]));
    if (myGraphs[selected].isScaleDirty()) updateLabels();
    updateGraph();
  } else console.log("Got message", msg.payloadString, msg.destinationName);
}

function onConnect() {
  console.log("connected");
  mqtt.subscribe("foo");
  mqtt.subscribe("aq");
  /*
  message = new Paho.MQTT.Message("Hello World");
  message.destinationName = "foo";
  mqtt.send(message);
  */
}

function updateGraph() {
  let d = myGraphs[selected].getFormula();
  line.setAttribute("d", d.std);
  shadow.setAttribute("d", d.shadow);
}

function updateLabels() {
  for (let i = 0; i <= 9; i++) {
    let el = document.getElementById("label" + i);
    el.textContent = ((9-i)*40/myGraphs[selected].scaley).toFixed();
  }
}

function positionLabels() {
  let x = 13;
  let r = graph.getBoundingClientRect();
  let rat = r.width/r.height;
  if (r.width > r.height) {
    x = 0;
  } else {
    x = GRAPHWIDTH - (GRAPHWIDTH * rat) + 1;
  }
  for (let i = 0; i <= 9; i++) {
    let el = document.getElementById("label" + i);
    el.setAttribute("x", x);
    el.setAttribute("y", 40*(i+1) - 2);
    
  }
}

const labels = [
  "PM1.0, CF=1",
	"PM2.5, CF=1",
	"PM10.  CF=1",
	"PM1.0",
	"PM2.5",
	"PM10.",
  "Particles < 0.3 micron",
  "Particles < 0.5 micron",
  "Particles < 1.0 micron",
  "Particles < 2.5 micron",
  "Particles < 5.0 micron",
  "Particles < 10. micron",
];

function updateSelected() {
  document.getElementById("sel").textContent = " Selected: " + selected + " (" + labels[selected] + ") ";
  updateGraph();
  updateLabels();
}

window.addEventListener("load", () => {
  line = document.getElementById("line");
  lineshadow = document.getElementById("shadow");
  /* Make the gridlines programatically */
  let gridline = document.getElementById("gridline");
  let gl = "";
  const count = 10;
  const dx = GRAPHWIDTH/parseFloat(count);
  const dy = GRAPHHEIGHT/parseFloat(count);
  for (let i = 0; i < count; i++) {
    gl += "M " + (dx * i) + " 0 L " + (dx * i) + " " + GRAPHHEIGHT;
    gl += " M 0 " + (dy * i) + " L " + GRAPHWIDTH  + " " + (dy * i);
    if (i < count-1) gl += " ";
  }
  gridline.setAttribute("d", gl);
  
  graph = document.getElementById("graph");
  window.addEventListener("resize", () => {
    positionLabels();
  });
  positionLabels();
  
  document.getElementById("next").addEventListener("click", ()=>{selected++; if (selected >= NUMGRAPHS) selected = 0; updateSelected();});
  document.getElementById("prev").addEventListener("click", ()=>{selected--; if (selected < 0) selected = NUMGRAPHS-1; updateSelected();});
  document.getElementById("sel").textContent = " Selected: " + selected + " (" + labels[selected] + ") ";
  
  mqtt = new Paho.MQTT.Client(host, port, "clientjs");
  mqtt.onMessageArrived = onMessage;
  mqtt.connect({timeout: 3, onSuccess: onConnect});
});
  </script>
  <style>
    .labelText{ font-family:Tahoma; font-size:12px; fill:#73726b; }
    * { font-size: 18px; }
  </style>
  <meta name='viewport' content='width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=0' >
  </head>
  <body>
    <div><button id="prev">Prev</button><span id="sel"></span><button id="next">Next</button></div>
    <svg id="graph" width="90vmin" height="90vmin" viewBox="0 0 400 400" preserveAspectRatio="xMaxYMid slice" style="background-color:#fffdf7;border-style:solid;border-color:black;">
      <defs>
        <linearGradient id="Gradient1" x1="0" x2="0" y1="0" y2="1">
          <stop offset="0%" stop-color="#36d402" stop-opacity="0.4"/>
          <stop offset="10%" stop-color="#36d402" stop-opacity="0.1"/>
          <stop offset="40%" stop-color="#36d402" stop-opacity="0"/>
        </linearGradient>
        <linearGradient id="Gradient2" x1="1" x2="0" y1="0" y2="0">
          <stop offset="100%" stop-color="#86f562"/>
          <stop offset="0%" stop-color="#36d402"/>
        </linearGradient>
      </defs>
      <path id = "gridline" stroke="#d1cfcb" stroke-width="1"/>
      <path id = "line" fill="none" stroke="#86f562" stroke-width="2"/>
      <path id = "shadow" fill="url(#Gradient1)" stroke-width="0"/>
      <text class="labelText" id = "label0"></text>
      <text class="labelText" id = "label1"></text>
      <text class="labelText" id = "label2"></text>
      <text class="labelText" id = "label3"></text>
      <text class="labelText" id = "label4"></text>
      <text class="labelText" id = "label5"></text>
      <text class="labelText" id = "label6"></text>
      <text class="labelText" id = "label7"></text>
      <text class="labelText" id = "label8"></text>
      <text class="labelText" id = "label9"></text>
  </body>
</html>
)"""";