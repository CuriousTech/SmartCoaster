const char page_index[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>WiFi Scale</title>
<style>
div,table{
border-radius: 2px;
margin-bottom: 2px;
box-shadow: 4px 4px 10px #000000;
background: rgb(160,160,160);
background: linear-gradient(0deg, rgba(94,94,94,1) 0%, rgba(160,160,160,1) 90%);
background-clip: padding-box;
}
input{
border-radius: 2px;
margin-bottom: 2px;
box-shadow: 4px 4px 10px #000000;
background: rgb(160,160,160);
background: linear-gradient(0deg, rgba(160,160,160,1) 0%, rgba(239,255,255,1) 100%);
background-clip: padding-box;
}
body{width:490px;display:block;text-align:right;font-family: Arial, Helvetica, sans-serif;}
</style>
<script src="http://ajax.googleapis.com/ajax/libs/jquery/1.6.1/jquery.min.js" type="text/javascript" charset="utf-8"></script>
<script type="text/javascript">
a=document.all
xPadding=40
yPadding=56
$(document).ready(function()
{
  openSocket()
})

function openSocket(){
  ws=new WebSocket("ws://"+window.location.host+"/ws")
//  ws=new WebSocket("ws://192.168.31.40/ws")
  ws.onopen=function(evt){setVar('hist',0)}
  ws.onclose=function(evt){alert("Connection closed");}
  ws.onmessage=function(evt){
  console.log(evt.data)
  d=JSON.parse(evt.data)
  switch(d.cmd)
  {
    case 'settings':
      a.SRATE.value=s2t(d.srate)
      a.LRATE.value=s2t(d.lrate)
      a.DRATE.value=d.drate
      a.WT.value=+d.wt
      a.EWT.value=+d.empty
      a.CAL.value=+d.cal
      break
    case 'state':
      dt=new Date(d.t*1000)
      a.time.innerHTML=dt.toLocaleTimeString()+' &nbsp; &nbsp; '+d.rssi+'dB  &nbsp; '

      g = +d.weight / 10
      oz = (g / 28.35).toFixed(1)
      floz = ((g - a.EWT.value) / 28.35 / 0.958611418535).toFixed(1)
      if(floz<0) floz=0
      a.ctr.innerHTML=floz+' flOz &nbsp; '+oz+' Oz &nbsp; '+g.toFixed(1)+' g &nbsp; '
      a.FLOZ.value=(+d.fla/10).toFixed(1)
      a.WD.value=(+d.wd/10).toFixed(1)
      break
    case 'alert':
      alert(d.text)
      break
    case 'ref':
      tb=d.tb
      arr=new Array()
      break
    case 'days':
      days=d.days
      break
    case 'data':
      for(i=0;i<d.d.length;i++){
        n=d.d[i][0]; d.d[i][0]=tb*1000; tb-=n
      }
      arr=arr.concat(d.d)
      draw()
      break
    case 'data2':
      d.d[0][0]*=1000
      arr=d.d.concat(arr)
      draw()
      break
  }
 }
}

function setVar(varName, value)
{
 ws.send('{"'+varName+'":'+value+'}')
}

function setSRate()
{
  setVar('srate', t2s(a.SRATE.value))
}
function setLRate()
{
  setVar('lrate', t2s(a.LRATE.value))
}
function setDRate()
{
  setVar('drate', a.DRATE.value)
}

function s2t(elap)
{
  m=Math.floor(elap/60)
  s=elap-(m*60)
  if(m==0) return s
  if(s<10) s='0'+s
  return m+':'+s
}

function t2s(v)
{
  if(typeof v=='string') v=(+v.substr(0, v.indexOf(':'))*60)+(+v.substr(v.indexOf(':')+1))
  return v
}

function draw(){
 graph = $('#chart')
 c=graph[0].getContext('2d')

 tipCanvas=document.getElementById("tip")
 tipCtx=tipCanvas.getContext("2d")
 tipDiv=document.getElementById("popup")

 c.fillStyle=c.strokeStyle='black'
 o=graph.offset()
 offsetX=o.left
 offsetY=o.top
 yPad=3
 c.lineWidth=2
 c.font='italic 8pt sans-serif'
 c.textAlign="left"
 c.clearRect(0, 0, graph.width(), graph.height())
 doBorder(graph)

 c.lineWidth = 1
 if(typeof(arr)=="undefined") return

 // value range
 c.textAlign="right"
 c.textBaseline="middle"

 // weight scale
 yRange=getMaxY()-getMinY()
 for(var i=getMinY();i<getMaxY();i+=(yRange/8))
  c.fillText((i/10).toFixed(1),graph.width()-6,getYPixel(i,getMinY()))

 // Weight
 c.strokeStyle=c.fillStyle='#f00'
 c.fillText('grams', graph.width()-6,6)
 drawArray(getMinY())

 graph2 = $('#chart2')
 ctx=graph2[0].getContext('2d')
  ctx.fillStyle="#FFF"
  ctx.font="10px sans-serif"
  dots=[]
  date = new Date()
  ctx.lineWidth=7
  draw_scale(days,graph2.width(),graph2.height(),0,date.getDate())

 // request mousemove events
 graph.mousemove(function(e){handleMouseMove(e);})

 // show tooltip when mouse hovers over anything
 function handleMouseMove(e){
  var c=document.getElementById('chart')
  rect=c.getBoundingClientRect()
  dx=e.clientX-rect.x-xPadding
  dy=e.clientY-rect.y

  tipCtx.clearRect(0,0,tipCanvas.width,tipCanvas.height)
  tipCtx.lineWidth=2
  tipCtx.fillStyle="#000000"
  tipCtx.strokeStyle='#333'
  tipCtx.font='italic 8pt sans-serif'
  tipCtx.textAlign="left"

  idx=arr.length-(dx*arr.length/(rect.width-xPadding*2)).toFixed()
  if(idx<0||idx>=arr.length)
    popup.style.left="-200px"
  else
  {
    tipCtx.fillText((new Date(arr[idx][0])).toLocaleTimeString()+' ',4,15)
    tipCtx.fillText((arr[idx][1]/10)+'g',4,29)
    popup=document.getElementById("popup")
    popup.style.top=(dy+rect.y+window.pageYOffset-30)+"px"
    popup.style.left=(dx+rect.x-40)+"px"
  }
 }

  // request mousemove events
  graph2.mousemove(function(e){handleMouseMove2(e);})

  // show tooltip when mouse hovers over dot
  function handleMouseMove2(e){
    var c2=document.getElementById('chart')
    rect=c2.getBoundingClientRect()
    mouseX=e.clientX-rect.x
    mouseY=e.clientY-rect.y
    var hit = false
    for(i=0;i<dots.length;i++){
      dot = dots[i]
      if(mouseX>=dot.x && mouseX<=dot.x2){
        tipCtx.clearRect(0,0,tipCanvas.width,tipCanvas.height)
        tipCtx.fillStyle="#FFA"
        tipCtx.lineWidth = 2
        tipCtx.fillStyle = "#000000"
        tipCtx.strokeStyle = '#333'
        tipCtx.font = 'bold italic 10pt sans-serif'
        tipCtx.textAlign = "left"
        tipCtx.fillText(dot.tip,4,15)
        hit=true
        popup=document.getElementById("popup")
        popup.style.top=(mouseY+rect.y+window.pageYOffset)+"px"
        popup.style.left=(dot.x+rect.x+20)+"px"
      }
    }
    if(!hit) popup.style.left="-200px"
  }

}

function drawArray(min)
{
  c.beginPath()
  c.moveTo(getXPixel(0),getYPixel(arr[0][1],min))
  for(var i=1;i<arr.length;i++)
   c.lineTo(getXPixel(i),getYPixel(arr[i][1],min))
  c.stroke()
}

function doBorder(g)
{
  c.beginPath()
  c.moveTo(1,0)
  c.lineTo(1,g.height()-yPad)
  c.lineTo(g.width()-xPadding, g.height()-yPad)
  c.lineTo(g.width()-xPadding, 0)
  c.stroke()
}

function getMaxY(){
  var max = 0
  
  for(i=0; i<arr.length-1; i++)
  {
    if(arr[i][1] > max)
      max=arr[i][1]
  }
  return Math.ceil(max)
}

function getMinY(){
  var min = 5000

  for(i=0; i<arr.length; i++)
  {
    if(arr[i][1]<min)
      min=arr[i][1]
  }
  return Math.floor(min)
}

function getXPixel(val){
  x=(graph.width()-xPadding)-((graph.width()-26-xPadding)/arr.length)*val
  return x.toFixed()
}

function getYPixel(val,min) {
  y=graph.height()-( ((graph.height()-yPad)/yRange)*(val-min))-yPad
  return y.toFixed()
}

function draw_scale(arr,w,h,o,hi)
{
  ctx.fillStyle="#777"
  ctx.fillRect(0,o,w-3,h-3)
  ctx.fillStyle="#000"
  max=0
  tot=0
  for(i=0;i<arr.length;i++)
  {
    if(arr[i]>max) max=arr[i]
    tot+=arr[i]
  }
  tot-=arr[0]
  ctx.textAlign="center"
  for(i=1;i<arr.length;i++)
  {
    x=i*(w/arr.length)-6
    ctx.strokeStyle="#55F"
    bh=arr[i]*(h-20)/max
    y=(o+h-20)-bh
    ctx.beginPath()
    ctx.moveTo(x,o+h-20)
    ctx.lineTo(x,y)
    ctx.stroke()
    ctx.fillText(i,x,o+h-7)
    if(i==hi) // current day
    {
      bh=arr[0]*(h-20)/max
      y=(o+h-20)-bh
      ctx.strokeStyle="rgba(235,160,160,100)"
      ctx.beginPath()
      ctx.moveTo(x,o+h-20)
      ctx.lineTo(x,y)
      ctx.stroke()
    }
    if(bh<16) bh=16
    dots.push({
      x: x-(ctx.lineWidth/2),
      x2: x+ctx.lineWidth,
      tip: arr[i]/10
    })
  }
  ctx.textAlign="right"
  tot/=10
  if(tot>1000)
   txt=(tot/1000).toFixed(1)+' KFO'
  else
   txt=tot.toFixed(1)+' FO'
  ctx.fillText(txt,w-3,o+10)
}

</script>
<style type="text/css">
#popup {
  position: absolute;
  top: 150px;
  left: -150px;
  z-index: 10;
  border-style: solid;
  border-width: 1px;
}
</style>
</head>
<body bgcolor="silver">
<table width=480>
<tr><td><div id='time'></div></td><td> <div id='ctr'></div></td></tr>
<tr>
<td>Update Rate<input id='SRATE' type=text size=4 value='10' onchange="{setSRate()}">sec</td>
<td>Cal weight:<input name="WT" type=text size=1 value='50'> g <input value="Calibrate" type='button' onclick="{setVar('calibrate', a.WT.value)}"> &nbsp; 
 <input value="Tare" type='button' onclick="{setVar('tare', 0)}"></td>
</tr>
<tr>
<td>Log Rate<input id='LRATE' type=text size=4 value='10' onchange="{setLRate()}">sec</td>
<td>Calibrate Value <input name="CAL" type=text size=2 value='1' onchange="{setVar('calval', this.value)}"></td>
</tr>
<tr>
<td>Dispaly Rate<input id='DRATE' type=text size=4 value='500' onchange="{setDRate()}"> ms</td>
<td></td>
</tr>
<tr>
<td colspan=2>
  Empty <input name="EWT" type=text size=2 value='260' onchange="{setVar('emptywt', this.value)}"> g &nbsp; 
 &nbsp; <input name="WD" type=text size=2 value='0' readonly> fl.oz. &nbsp; <input name="FLOZ" type=text size=2 value='1' onchange="{setVar('floz', this.value)}"> fl.oz. Total &nbsp; <input value="Reset" type=button onclick="{setVar('clear', 0)}">
</td>
</tr>
</table>

<table width=480>
<tr><td>
<div id="wrapper">
<canvas id="chart" width="474" height="260"></canvas>
<canvas id="chart2" width="474" height="120"></canvas>
<div id="popup"><canvas id="tip" width=70 height=32></canvas></div>
</div>
</td></tr>
</table></body>
</html>
)rawliteral";

const uint8_t favicon[] PROGMEM = {
  0x1F, 0x8B, 0x08, 0x08, 0x70, 0xC9, 0xE2, 0x59, 0x04, 0x00, 0x66, 0x61, 0x76, 0x69, 0x63, 0x6F, 
  0x6E, 0x2E, 0x69, 0x63, 0x6F, 0x00, 0xD5, 0x94, 0x31, 0x4B, 0xC3, 0x50, 0x14, 0x85, 0x4F, 0x6B, 
  0xC0, 0x52, 0x0A, 0x86, 0x22, 0x9D, 0xA4, 0x74, 0xC8, 0xE0, 0x28, 0x46, 0xC4, 0x41, 0xB0, 0x53, 
  0x7F, 0x87, 0x64, 0x72, 0x14, 0x71, 0xD7, 0xB5, 0x38, 0x38, 0xF9, 0x03, 0xFC, 0x05, 0x1D, 0xB3, 
  0x0A, 0x9D, 0x9D, 0xA4, 0x74, 0x15, 0x44, 0xC4, 0x4D, 0x07, 0x07, 0x89, 0xFA, 0x3C, 0x97, 0x9C, 
  0xE8, 0x1B, 0xDA, 0x92, 0x16, 0x3A, 0xF4, 0x86, 0x8F, 0x77, 0x73, 0xEF, 0x39, 0xEF, 0xBD, 0xBC, 
  0x90, 0x00, 0x15, 0x5E, 0x61, 0x68, 0x63, 0x07, 0x27, 0x01, 0xD0, 0x02, 0xB0, 0x4D, 0x58, 0x62, 
  0x25, 0xAF, 0x5B, 0x74, 0x03, 0xAC, 0x54, 0xC4, 0x71, 0xDC, 0x35, 0xB0, 0x40, 0xD0, 0xD7, 0x24, 
  0x99, 0x68, 0x62, 0xFE, 0xA8, 0xD2, 0x77, 0x6B, 0x58, 0x8E, 0x92, 0x41, 0xFD, 0x21, 0x79, 0x22, 
  0x89, 0x7C, 0x55, 0xCB, 0xC9, 0xB3, 0xF5, 0x4A, 0xF8, 0xF7, 0xC9, 0x27, 0x71, 0xE4, 0x55, 0x38, 
  0xD5, 0x0E, 0x66, 0xF8, 0x22, 0x72, 0x43, 0xDA, 0x64, 0x8F, 0xA4, 0xE4, 0x43, 0xA4, 0xAA, 0xB5, 
  0xA5, 0x89, 0x26, 0xF8, 0x13, 0x6F, 0xCD, 0x63, 0x96, 0x6A, 0x5E, 0xBB, 0x66, 0x35, 0x6F, 0x2F, 
  0x89, 0xE7, 0xAB, 0x93, 0x1E, 0xD3, 0x80, 0x63, 0x9F, 0x7C, 0x9B, 0x46, 0xEB, 0xDE, 0x1B, 0xCA, 
  0x9D, 0x7A, 0x7D, 0x69, 0x7B, 0xF2, 0x9E, 0xAB, 0x37, 0x20, 0x21, 0xD9, 0xB5, 0x33, 0x2F, 0xD6, 
  0x2A, 0xF6, 0xA4, 0xDA, 0x8E, 0x34, 0x03, 0xAB, 0xCB, 0xBB, 0x45, 0x46, 0xBA, 0x7F, 0x21, 0xA7, 
  0x64, 0x53, 0x7B, 0x6B, 0x18, 0xCA, 0x5B, 0xE4, 0xCC, 0x9B, 0xF7, 0xC1, 0xBC, 0x85, 0x4E, 0xE7, 
  0x92, 0x15, 0xFB, 0xD4, 0x9C, 0xA9, 0x18, 0x79, 0xCF, 0x95, 0x49, 0xDB, 0x98, 0xF2, 0x0E, 0xAE, 
  0xC8, 0xF8, 0x4F, 0xFF, 0x3F, 0xDF, 0x58, 0xBD, 0x08, 0x25, 0x42, 0x67, 0xD3, 0x11, 0x75, 0x2C, 
  0x29, 0x9C, 0xCB, 0xF9, 0xB9, 0x00, 0xBE, 0x8E, 0xF2, 0xF1, 0xFD, 0x1A, 0x78, 0xDB, 0x00, 0xEE, 
  0xD6, 0x80, 0xE1, 0x90, 0xFF, 0x90, 0x40, 0x1F, 0x04, 0xBF, 0xC4, 0xCB, 0x0A, 0xF0, 0xB8, 0x6E, 
  0xDA, 0xDC, 0xF7, 0x0B, 0xE9, 0xA4, 0xB1, 0xC3, 0x7E, 0x04, 0x00, 0x00, 
};
