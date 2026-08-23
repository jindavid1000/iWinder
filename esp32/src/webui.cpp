#include "webui.h"
#include <WebServer.h>
#include "protocol.h"
#include "comms.h"
#include "state.h"
#include "config.h"

WebUi g_webui;

static WebServer s_server(80);

// ============================================================================
//  内嵌页面（PROGMEM，gzip 可选——当前体积可直接明文）
//  移动优先单页应用: 控制页 + 参数页，500ms 轮询状态。
// ============================================================================
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>iWinder 绕线器</title>
<style>
:root{--bg:#101418;--card:#1a2028;--line:#2a323d;--tx:#e8ecf1;--dim:#8b98a5;
--ac:#4f9cf9;--ok:#3fb27f;--warn:#e8a33d;--err:#e05252;--r:14px}
*{box-sizing:border-box;margin:0;padding:0;-webkit-tap-highlight-color:transparent}
body{background:var(--bg);color:var(--tx);font:15px/1.5 system-ui,-apple-system,"PingFang SC","Microsoft YaHei",sans-serif;
max-width:560px;margin:0 auto;padding:12px 12px 40px}
.tabs{display:flex;gap:8px;margin-bottom:12px}
.tabs button{flex:1;padding:10px;border:1px solid var(--line);border-radius:var(--r);
background:var(--card);color:var(--dim);font-size:15px}
.tabs button.on{color:var(--tx);border-color:var(--ac);background:#1c2735}
.card{background:var(--card);border:1px solid var(--line);border-radius:var(--r);
padding:14px;margin-bottom:12px}
.card h3{font-size:14px;color:var(--dim);font-weight:600;margin-bottom:10px;letter-spacing:.5px}
.state{display:flex;align-items:center;gap:10px;margin-bottom:10px}
.dot{width:12px;height:12px;border-radius:50%;background:var(--dim);flex:none}
.dot.run{background:var(--ok);box-shadow:0 0 8px var(--ok)}
.dot.err{background:var(--err);box-shadow:0 0 8px var(--err)}
.dot.mid{background:var(--warn)}
.state b{font-size:17px}
.state span{margin-left:auto;color:var(--dim);font-size:12px}
.grid{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}
.kv{background:#141a21;border-radius:10px;padding:8px 6px;text-align:center}
.kv i{display:block;font-style:normal;font-size:11px;color:var(--dim)}
.kv b{font-size:16px;font-weight:600}
.btns{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}
button.act{padding:12px 8px;border:none;border-radius:12px;font-size:15px;
background:#263140;color:var(--tx);cursor:pointer}
button.act:active{transform:scale(.97)}
button.act:disabled{opacity:.35}
#bStart{background:var(--ok);color:#fff}
#bStop{background:var(--err);color:#fff}
.slider{margin-top:12px}
.slider .row{display:flex;justify-content:space-between;margin-bottom:6px;font-size:13px;color:var(--dim)}
.slider b{color:var(--tx);font-size:16px}
input[type=range]{width:100%;height:34px;accent-color:var(--ac)}
.errbox{background:#3a1d1d;border:1px solid var(--err);color:#ffb4b4;
border-radius:10px;padding:10px 12px;margin-bottom:12px;display:none;font-size:13px}
.errbox button{margin-top:6px}
.frow{display:flex;align-items:center;gap:8px;margin-bottom:8px}
.frow label{flex:0 0 42%;font-size:13px;color:var(--dim)}
.frow input,.frow select{flex:1;min-width:0;background:#141a21;border:1px solid var(--line);
border-radius:8px;color:var(--tx);padding:8px 10px;font-size:14px}
.frow input:focus,.frow select:focus{outline:none;border-color:var(--ac)}
.save{width:100%;padding:12px;border:none;border-radius:12px;background:var(--ac);
color:#fff;font-size:15px;margin-top:4px}
.calib{width:100%;padding:12px;border:1px solid var(--line);border-radius:12px;
background:transparent;color:var(--tx);font-size:14px;margin-top:8px}
.tip{font-size:12px;color:var(--dim);line-height:1.6;margin-top:8px}
.toast{position:fixed;left:50%;bottom:24px;transform:translateX(-50%);background:#2a323d;
padding:10px 20px;border-radius:20px;font-size:13px;opacity:0;transition:.3s;pointer-events:none}
.toast.show{opacity:1}
.bar{height:26px;background:#141a21;border-radius:8px;overflow:hidden;position:relative;margin-top:10px}
.bar i{position:absolute;left:0;top:0;bottom:0;background:linear-gradient(90deg,#2d6fd2,#4f9cf9);border-radius:8px}
.bar em{position:absolute;inset:0;display:flex;align-items:center;justify-content:center;
font-style:normal;font-size:11px;color:#fff;text-shadow:0 1px 2px #000a}
</style>
</head>
<body>
<div class="tabs">
<button id="tabCtl" class="on" onclick="tab(0)">控制</button>
<button id="tabPrm" onclick="tab(1)">参数</button>
</div>

<div id="pageCtl">
<div class="errbox" id="errbox"><b id="errcode"></b> <span id="errmsg"></span><br>
<button class="act" onclick="cmd({cmd:'clear_error'})">清除错误</button></div>

<div class="card">
<div class="state"><div class="dot" id="dot"></div><b id="stateTxt">--</b>
<span id="linkTxt"></span></div>
<div class="grid">
<div class="kv"><i>转速 RPM</i><b id="rpm">0</b></div>
<div class="kv"><i>圈数</i><b id="turns">0</b></div>
<div class="kv"><i>长度 m</i><b id="len">0</b></div>
<div class="kv"><i>排线位置 mm</i><b id="pos">0</b></div>
<div class="kv"><i>来回数</i><b id="trips">0</b></div>
<div class="kv"><i>层数</i><b id="layer">0</b></div>
</div>
<div class="bar"><i id="posBar"></i><em id="posTxt"></em></div>
</div>

<div class="card" id="speedCard">
<h3>运行速度</h3>
<div class="slider"><div class="row"><span>电机转速</span><b id="spdTxt">50%</b></div>
<input type="range" min="0" max="100" value="50" id="spd"></div>
</div>

<div class="card" id="manualTip" style="display:none">
<h3>手动模式</h3>
<div class="tip">电机不输出，请手摇驱动料盘，排线自动跟随转速。
停转约 2 秒（来回数达标）会自动校准排线位置。</div>
</div>

<div class="card">
<div class="btns">
<button class="act" id="bStart" onclick="doStart()">启动</button>
<button class="act" id="bPause" onclick="cmd({cmd:'pause'})">暂停</button>
<button class="act" id="bResume" onclick="cmd({cmd:'resume'})">恢复</button>
<button class="act" onclick="cmd({cmd:'home'})">回原点</button>
<button class="act" id="bStop" onclick="cmd({cmd:'stop'})">停止</button>
</div>
</div>
</div>

<div id="pagePrm" style="display:none">
<div class="card">
<h3>驱动与绕线</h3>
<div class="frow"><label>驱动模式</label><select id="p_driveMode">
<option value="0">电动（电机）</option><option value="1">手动（手摇）</option></select></div>
<div class="frow"><label>电机驱动电路</label><select id="p_motorDriver">
<option value="0">MOS 管调速</option><option value="1">L298N 开关（不调速）</option></select></div>
<div class="frow"><label>左起始位置 mm</label><input type="number" step="0.5" id="p_traverseLeftStart"></div>
<div class="frow"><label>右终止位置 mm</label><input type="number" step="0.5" id="p_traverseRightEnd"></div>
<div class="frow"><label>限位间距 mm</label><input type="number" step="0.5" id="p_travelRangeMm"></div>
<div class="frow"><label>线径 mm</label><input type="number" step="0.01" id="p_filamentDiameter"></div>
<div class="frow"><label>料盘宽度 mm</label><input type="number" step="1" id="p_spoolWidth"></div>
<div class="frow"><label>校准间隔 (来回)</label><input type="number" step="1" id="p_calIntervalRounds"></div>
<div class="tip">绕线宽度 = 右终止 − 左起始（料盘宽度只用于长度计算）。</div>
</div>
<div class="card">
<h3>传感器</h3>
<div class="frow"><label>料盘磁铁数</label><input type="number" step="1" id="p_hallSpoolMagnets"></div>
<div class="frow"><label>霍尔去抖 us</label><input type="number" step="1000" id="p_hallDebounceUs"></div>
</div>
<div class="card">
<h3>排线编码器</h3>
<div class="frow"><label>位置反馈</label><select id="p_traverseEncoder">
<option value="0">舵机开环估算</option><option value="1">AS5600 闭环</option></select></div>
<div class="frow"><label>增速齿比</label><input type="number" step="0.1" id="p_encGearRatio"></div>
</div>
<div class="card">
<h3>WiFi 配网</h3>
<div class="frow"><label>WiFi 名称</label><input id="w_ssid" placeholder="家庭 WiFi SSID"></div>
<div class="frow"><label>WiFi 密码</label><input id="w_pass" type="password" placeholder="WiFi 密码"></div>
<button class="save" onclick="sendWifi()">发送配网</button>
<div class="tip">配网后设备接入家庭 WiFi（连接需数秒~半分钟，本页面期间可能短暂无响应）。
热点模式下 <b>iwinder.local 不可用属正常</b>——请始终用 192.168.4.1 访问热点；
连入家庭 WiFi 后同一网络的 iOS/macOS 才能用 http://iwinder.local，其他设备从状态页查看 IP。</div>
</div>
<button class="save" onclick="saveParams()">保存并下发</button>
<button class="calib" onclick="if(confirm('排线将满速运动数个来回进行标定，确认开始？'))cmd({cmd:'calibrate_servo'})">
舵机速度标定</button>
<div class="tip">标定需在待机状态进行；装了 AS5600 会同时标定编码器每圈位移。</div>
</div>

<div class="toast" id="toast"></div>
<script>
const $=id=>document.getElementById(id);
const FIELDS={driveMode:'p_driveMode',motorDriver:'p_motorDriver',traverseLeftStart:'p_traverseLeftStart',traverseRightEnd:'p_traverseRightEnd',
travelRangeMm:'p_travelRangeMm',filamentDiameter:'p_filamentDiameter',spoolWidth:'p_spoolWidth',
calIntervalRounds:'p_calIntervalRounds',hallSpoolMagnets:'p_hallSpoolMagnets',hallDebounceUs:'p_hallDebounceUs',
traverseEncoder:'p_traverseEncoder',encGearRatio:'p_encGearRatio'};
let stateName='idle';

function tab(i){tabCtl.classList.toggle('on',i==0);tabPrm.classList.toggle('on',i==1);
pageCtl.style.display=i==0?'':'none';pagePrm.style.display=i==1?'':'none';}

function toast(t){const e=$('toast');e.textContent=t;e.classList.add('show');
clearTimeout(e._t);e._t=setTimeout(()=>e.classList.remove('show'),1800);}

async function cmd(o){
try{const r=await fetch('/api/cmd',{method:'POST',
headers:{'Content-Type':'application/json'},body:JSON.stringify(o)});
const j=await r.json();if(j.ok===false)toast('失败: '+(j.msg||''));else toast('已发送');
poll();}catch(e){toast('发送失败');}
}
function doStart(){cmd({cmd:'start',speed:manualMode?0:+$('spd').value});}

async function sendWifi(){
const ssid=$('w_ssid').value.trim(),pass=$('w_pass').value;
if(!ssid){toast('请填写 WiFi 名称');return;}
toast('正在发送配网...');
try{const r=await fetch('/api/cmd',{method:'POST',headers:{'Content-Type':'application/json'},
body:JSON.stringify({cmd:'set_wifi',ssid:ssid,password:pass})});
const j=await r.json();
toast(j.ok?'配网成功，设备正在连接 '+ssid:'失败: '+(j.msg||'未知错误'));}
catch(e){toast('发送失败，设备可能正在重连网络');}}

let manualMode=false,dragT=0;
$('spd').addEventListener('input',e=>{
$('spdTxt').textContent=e.target.value+'%';
clearTimeout(dragT);dragT=setTimeout(()=>cmd({cmd:'set_speed',speed:+e.target.value}),250);});

const STATE_CN={idle:'待机',homing:'寻原点',positioning:'定位中',running:'绕线中',paused:'已暂停',
calibrating:'校准中',error:'异常',completed:'已完成',servo_calib:'舵机标定中'};

async function poll(){
try{
const r=await fetch('/api/status');const s=await r.json();
stateName=s.state;
manualMode=(s.drive_mode==1);
$('speedCard').style.display=manualMode?'none':'';
$('manualTip').style.display=manualMode?'':'none';
$('dot').className='dot '+(s.state=='running'?'run':s.state=='error'?'err':
(s.state=='idle'||s.state=='completed')?'':'mid');
$('stateTxt').textContent=STATE_CN[s.state]||s.state;
$('linkTxt').textContent='运行 '+Math.floor(s.uptime/60)+' 分钟';
$('rpm').textContent=s.spool_rpm.toFixed(0);
$('turns').textContent=s.spool_turns.toFixed(0);
$('len').textContent=s.length.toFixed(1);
$('pos').textContent=s.traverse_pos.toFixed(1);
$('trips').textContent=s.round_trips;
$('layer').textContent=s.current_layer;
const L=+$('p_traverseLeftStart').value||0,R=+$('p_traverseRightEnd').value||80;
const p=Math.min(100,Math.max(0,(s.traverse_pos-L)/(R-L)*100));
$('posBar').style.width=p.toFixed(1)+'%';
$('posTxt').textContent=s.traverse_pos.toFixed(1)+' / '+(manualMode?'':'')+'mm '+(s.traverse_dir=='left'?'◀':s.traverse_dir=='right'?'▶':'·');
const eb=$('errbox');
if(s.state=='error'){eb.style.display='block';$('errcode').textContent='['+s.error_code+']';
$('errmsg').textContent=s.error_msg||'';}else eb.style.display='none';
$('bStart').disabled=!(stateName=='idle'||stateName=='completed');
$('bPause').disabled=stateName!='running';
$('bResume').disabled=stateName!='paused';
$('bStop').disabled=(stateName=='idle'||stateName=='completed');
}catch(e){$('stateTxt').textContent='连接断开';$('dot').className='dot';}
}

async function loadParams(){
try{const r=await fetch('/api/params');const p=await r.json();
for(const k in FIELDS){const e=$(FIELDS[k]);if(e&&p[k]!==undefined)e.value=p[k];}
}catch(e){}
}
async function saveParams(){
const o={};
for(const k in FIELDS){const e=$(FIELDS[k]);if(e&&e.value!=='')o[k]=+e.value;}
o.cmd='set_params';o.params={};for(const k in FIELDS)o.params[k]=+$(FIELDS[k]).value;
try{const r=await fetch('/api/cmd',{method:'POST',headers:{'Content-Type':'application/json'},
body:JSON.stringify(o)});toast('参数已下发');}catch(e){toast('下发失败');}
}

loadParams();poll();setInterval(poll,500);
</script>
</body>
</html>
)rawliteral";

// ============================================================================
//  HTTP 路由
// ============================================================================

static void handleIndex() {
    s_server.send_P(200, "text/html", INDEX_HTML);
}

static void handleStatus() {
    s_server.send(200, "application/json", g_protocol.buildStatusJson());
}

static void handleParams() {
    s_server.send(200, "application/json", getConfigJson(g_config));
}

// 所有命令复用 TCP 协议处理器: {"cmd":"start","speed":100} / {"cmd":"set_params","params":{...}}
static void handleCmd() {
    if (!s_server.hasArg("plain")) {
        s_server.send(400, "application/json", "{\"ok\":false,\"msg\":\"缺少请求体\"}");
        return;
    }
    g_protocol.handle(s_server.arg("plain"));
    // 响应通过状态轮询体现，这里统一回 OK
    s_server.send(200, "application/json", "{\"ok\":true}");
}

static void handleNotFound() {
    s_server.send(404, "application/json", "{\"ok\":false}");
}

void WebUi::begin() {
    if (_begun) return;
    s_server.on("/", HTTP_GET, handleIndex);
    s_server.on("/api/status", HTTP_GET, handleStatus);
    s_server.on("/api/params", HTTP_GET, handleParams);
    s_server.on("/api/cmd", HTTP_POST, handleCmd);
    s_server.onNotFound(handleNotFound);
    s_server.begin();
    _begun = true;
    Serial.println("[WebUi] HTTP 服务已启动 (端口 80)");
}

void WebUi::update() {
    if (_begun) s_server.handleClient();
}
