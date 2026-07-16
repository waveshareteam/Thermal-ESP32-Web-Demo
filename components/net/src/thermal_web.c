/*****************************************************************************
 * @file     thermal_web.c
 * @version  1.0
 * @brief    Thermal web streaming server - serves HTML page and binary frame data
 * @date     16 Jul 2026
 ******************************************************************************/
#include "thermal_web.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <string.h>
#include <stdint.h>
#include "SenXorLib.h"
#include "Customer_Interface.h"

extern bool senxorTask_GetLatestFrame(uint16_t *frame, size_t framePixelCount, uint32_t *frameNumber);

EXT_RAM_BSS_ATTR static uint16_t g_FrameBuf[5040];

#define THERMAL_WEB_TAG "[THERMAL_WEB]"

/* Thermal frame dimensions */
#define THERMAL_COLS 80
#define THERMAL_ROWS 62
/* Full frame buffer: 80-word header + 80x62 image = 5040 uint16_t */
#define THERMAL_FRAME_SIZE (80 * 63)
#define THERMAL_IMAGE_OFFSET 80
#define THERMAL_IMAGE_SIZE (THERMAL_COLS * THERMAL_ROWS) /* 4960 uint16_t */

/*
 * Forward declaration of senxorFrame (defined in main/include/senxorTask.h).
 * We only need the struct layout to access the raw frame data.
 */
typedef struct {
    uint16_t mFrame[THERMAL_FRAME_SIZE];
} senxorFrame;

/* External queue handle �?populated by senxorTask */
extern QueueHandle_t senxorFrameQueue;
/* Debug counters */
static uint32_t g_frame_req_count = 0;
static uint32_t g_frame_sent_count = 0;
static uint32_t g_frame_empty_count = 0;
/*
 * Embedded HTML page for the thermal viewer (single-page application).
 * The page renders an 80x63 thermal frame on an HTML5 Canvas.
 * Colormaps are generated algorithmically in JS to keep the HTML compact.
 */
static const char THERMAL_HTML_PAGE[] =
"<!DOCTYPE html><html lang=\"en\"><head>"
"<meta charset=\"UTF-8\">"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1.0\">"
"<title>Thermal Camera</title>"
"<style>"
"*{margin:0;padding:0;box-sizing:border-box}"
"body{font-family:system-ui,sans-serif;background:#0a0a0f;color:#ddd;"
"display:flex;flex-direction:column;align-items:center;padding:12px;min-height:100vh}"
"h1{font-size:1.2rem;font-weight:400;letter-spacing:1px;margin:8px 0}"
".dot{width:10px;height:10px;border-radius:50%;display:inline-block;background:#f44;margin-right:6px}"
".dot.ok{background:#4f4}"
"canvas{border:2px solid #1a1a2e;border-radius:6px;image-rendering:pixelated;max-width:100%}"
".info{display:flex;gap:16px;margin:8px 0;font-size:.8rem;flex-wrap:wrap;justify-content:center}"
".info span{padding:4px 10px;background:#111122;border:1px solid #1a1a2e;border-radius:4px}"
".info b{color:#69afff}"
".scale{display:flex;align-items:center;gap:8px;font-size:.7rem;color:#888;margin:4px 0}"
".grad{width:180px;height:12px;border-radius:2px;"
"background:linear-gradient(90deg,#000002,#0b0036,#3b0f6f,#8c2981,#d94f5c,#f98e4b,#fdc328,#fcffa4)}"
".btns{margin:8px 0;display:flex;gap:6px;flex-wrap:wrap;justify-content:center}"
".btns button{background:#16162a;color:#aaa;border:1px solid #2a2a4a;"
"padding:5px 12px;border-radius:4px;cursor:pointer;font-size:.75rem}"
".btns button.on{background:#1a3355;border-color:#48f;color:#fff}"
"#fps{font-size:.7rem;color:#555;margin:2px 0}"
"</style></head><body>"
"<h1><span class=\"dot\" id=\"d\"></span>Thermal Camera <small id=\"st\">Live</small></h1>"
"<canvas id=\"c\"></canvas>"
"<div class=\"scale\"><span id=\"smin\">--</span><div class=\"grad\"></div><span id=\"smax\">--</span></div>"
"<div class=\"info\">"
"<span>Min <b id=\"tmin\">--</b>&deg;C</span>"
"<span>Max <b id=\"tmax\">--</b>&deg;C</span>"
"<span>Ctr <b id=\"tctr\">--</b>&deg;C</span>"
"</div>"
"<div id=\"fps\">FPS: --</div>"
"<div id=\"dbg\" style=\"font-size:.7rem;color:#ff0;margin:4px 0;min-height:1.2em\">Connecting...</div>"
"<div class=\"btns\">"
"<button class=\"on\" onclick=\"CM=0;U()\">Inferno</button>"
"<button onclick=\"CM=1;U()\">Ironbow</button>"
"<button onclick=\"CM=2;U()\">Rainbow</button>"
"<button onclick=\"CM=3;U()\">Gray</button>"
"<button class=\"on\" id=\"ba\" onclick=\"AS=!AS;U()\">Auto</button>"
"</div>"
"<script>"
"var W=80,H=62,S=4,CM=0,AS=!0,gMin=1e9,gMax=-1e9,lt=0,fc=0,ft=0,NR=3,Tfmt=0,Tres=10,ema=null;"
"var cv=document.getElementById('c'),cx=cv.getContext('2d');"
"cv.width=W*S;cv.height=H*S;"
"function mc(s){"
"var a=new Uint8Array(768),i,j,lt,p0,r0,g0,b0,p1,r1,g1,b1;"
"for(i=0;i<256;i++){"
"for(j=0;j<s.length-4&&i>s[j+4];j+=4);"
"p0=s[j];r0=s[j+1];g0=s[j+2];b0=s[j+3];"
"p1=s[j+4];r1=s[j+5];g1=s[j+6];b1=s[j+7];"
"lt=(i-p0)/(p1-p0+0.001);"
"a[i]=r0+(r1-r0)*lt|0;"
"a[256+i]=g0+(g1-g0)*lt|0;"
"a[512+i]=b0+(b1-b0)*lt|0;}"
"return a;}"
"var cmaps=["
"mc([0,0,0,4, 51,60,7,84, 128,180,50,120, 204,240,140,70, 255,255,255,180]),"
"mc([0,0,0,0, 64,0,0,255, 128,255,0,0, 192,255,255,0, 255,255,255,255]),"
"mc([0,0,0,0, 51,0,0,255, 102,0,255,0, 153,255,255,0, 204,255,0,0, 255,255,255,255]),"
"mc([0,0,0,0, 255,255,255,255])"
"];"
"function K(v){"
"if(Tfmt==2)return(v/Tres-32)*5/9;"
"if(Tfmt==1)return v/Tres;"
"return v/Tres-273.15;}"
"function U(){"
"var b=document.getElementById('ba');"
"var i,btns=document.querySelectorAll('.btns button');"
"for(i=0;i<4;i++)btns[i].className=(i===CM?'on':'');"
"b.className=AS?'on':'';}"
"function R(d){"
"if(!d||d.length<W*H)return;"
"var A=[0,0.15,0.35,0.55][NR];"
"if(!ema||ema.length!=d.length){ema=new Float32Array(d.length);for(var i=0;i<d.length;i++)ema[i]=d[i];}"
"else if(A>0){for(var i=0;i<d.length;i++){if(d[i]<65500)ema[i]=A*d[i]+(1-A)*ema[i];}}"
"else{for(var i=0;i<d.length;i++)ema[i]=d[i];}"
"var i,v,mn=1e9,mx=-1e9,good=0;"
"for(i=0;i<W*H;i++){v=ema[i];if(v<65500){if(v<mn)mn=v;if(v>mx)mx=v;good++;}}"
"if(good<10)return;"
"if(mn>=mx)mx=mn+1;"
"if(mn<gMin)gMin=mn;if(mx>gMax)gMax=mx;"
"var r0=AS?mn:gMin,r1=AS?mx:gMax;"
"if(r1<=r0)r1=r0+1;"
"var rng=r1-r0,cm=cmaps[CM],img=cx.createImageData(W,H);"
"for(var y=0;y<H;y++)for(var x=0;x<W;x++){"
"var vi=ema[y*W+x];if(vi>65500)vi=r0;"
"var ti=((vi-r0)/rng)*255;if(ti<0)ti=0;if(ti>255)ti=255;"
"ti=ti|0;var p=(y*W+x)*4;"
"img.data[p]=cm[ti];img.data[p+1]=cm[256+ti];img.data[p+2]=cm[512+ti];img.data[p+3]=255;}"
"var oc=document.createElement('canvas');oc.width=W;oc.height=H;"
"oc.getContext('2d').putImageData(img,0,0);"
"cx.clearRect(0,0,cv.width,cv.height);cx.drawImage(oc,0,0,W*S,H*S);"
"document.getElementById('tmin').textContent=K(mn).toFixed(1);"
"document.getElementById('tmax').textContent=K(mx).toFixed(1);"
"document.getElementById('smin').textContent=K(r0).toFixed(1)+'\\u00b0C';"
"document.getElementById('smax').textContent=K(r1).toFixed(1)+'\\u00b0C';"
"var cx2=Math.floor(W/2),cy2=Math.floor(H/2);"
"document.getElementById('tctr').textContent=K(ema[cy2*W+cx2]).toFixed(1);}"
"function P(){"
"fetch('/thermal/frame').then(function(r){"
"if(!r.ok)throw Error('HTTP '+r.status);"
"var h=r.headers.get('X-Temp-Res');"
"if(h){var p=h.split(',');Tres=+p[0]||10;Tfmt=+p[1]||0;}"
"document.getElementById('d').className='dot ok';"
"document.getElementById('st').textContent='Live';"
"document.getElementById('dbg').textContent='';"
"return r.arrayBuffer();})"
".then(function(b){var n=performance.now();"
"if(lt){fc++;if(n-ft>=1000){var fps=Math.round(fc*1000/(n-ft));"
"document.getElementById('fps').textContent='FPS: '+fps;fc=0;ft=n;}}else ft=n;lt=n;"
"var d=new Uint16Array(b);"
"R(d);})"
".catch(function(e){"
"document.getElementById('d').className='dot';"
"document.getElementById('st').textContent='Offline';})"
".finally(function(){setTimeout(P,50);});}"
"P();"
"</script></body></html>";
/* ────────────────────────────────────────────────────────────────
 * Embedded HTML page for raw temperature matrix viewer
 * ──────────────────────────────────────────────────────────────── */
static const char THERMAL_RAW_PAGE[] =
"<!DOCTYPE html><html><head>"
"<meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
"<title>Thermal Raw Matrix</title>"
"<style>"
"*{margin:0;padding:0;box-sizing:border-box}"
"body{font-family:monospace;background:#0a0a0f;color:#ccc;padding:8px}"
"h2{font-size:1rem;margin:4px 0;color:#69afff}"
".info{font-size:.75rem;color:#888;margin:4px 0}"
"table{border-collapse:collapse;font-size:9px}"
"td{width:22px;height:16px;text-align:center;border:1px solid #1a1a2e;padding:0;overflow:hidden}"
"#wrap{overflow:auto;max-height:90vh}"
"</style></head><body>"
"<h2>Thermal Raw Matrix (80x62)</h2>"
"<div class=\"info\">Values: raw sensor data | <span id=\"fmt\"></span> | <span id=\"st\">Loading...</span></div>"
"<div id=\"wrap\"><table id=\"tbl\"></table></div>"
"<script>"
"var W=80,H=62,Tfmt=0,Tres=10;"
"var tbl=document.getElementById('tbl');"
"function K(v){if(Tfmt==1)return v/Tres;return v/Tres-273.15;}"
"function heat(v,mn,mx){"
"if(v>65500)return'#000';"
"var t=(v-mn)/(mx-mn+0.001);"
"if(t<0)t=0;if(t>1)t=1;"
"var r=t<0.5?0:(t-0.5)*510,b=t<0.5?255-t*510:0;"
"return'rgb('+(r|0)+','+((128-t*128)|0)+','+(b|0)+')';}"
"function build(d){"
"var i,v,mn=1e9,mx=-1e9,good=0;"
"for(i=0;i<W*H;i++){v=d[i];if(v<65500){if(v<mn)mn=v;if(v>mx)mx=v;good++;}}"
"if(good<10)return;"
"if(mn>=mx)mx=mn+1;"
"var h='';"
"for(var y=0;y<H;y++){h+='<tr>';"
"for(var x=0;x<W;x++){"
"v=d[y*W+x];var c=heat(v,mn,mx);"
"h+='<td title=\"'+K(v).toFixed(1)+'C\" style=\"background:'+c+';color:'+(v>(mn+mx)/2?'#000':'#fff')+'\">'+v+'</td>';}"
"h+='</tr>';}"
"tbl.innerHTML=h;"
"document.getElementById('st').textContent='min='+mn+'('+K(mn).toFixed(1)+'C) max='+mx+'('+K(mx).toFixed(1)+'C)';"
"document.getElementById('fmt').textContent='Tfmt:'+Tfmt+' Tres:'+Tres;}"
"function P(){"
"fetch('/thermal/frame').then(function(r){"
"if(!r.ok)throw Error(r.status);"
"var h=r.headers.get('X-Temp-Res');"
"if(h){var p=h.split(',');Tres=+p[0]||10;Tfmt=+p[1]||0;}"
"return r.arrayBuffer();})"
".then(function(b){build(new Uint16Array(b));})"
".catch(function(e){document.getElementById('st').textContent='ERR:'+e.message;})"
".finally(function(){setTimeout(P,500);});}"
"P();"
"</script></body></html>";
/* ────────────────────────────────────────────────────────────────
 * HTTP Handlers
 * ──────────────────────────────────────────────────────────────── */

/**
 * @brief Handler for GET /thermal - serves the HTML viewer page
 */
static esp_err_t thermal_page_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_sendstr(req, THERMAL_HTML_PAGE);
    return ESP_OK;
}

/**
 * @brief Handler for GET /thermal/raw - serves the raw temperature matrix page
 */
static esp_err_t thermal_raw_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_sendstr(req, THERMAL_RAW_PAGE);
    return ESP_OK;
}

/**
 * @brief Handler for POST /thermal/start - starts sensor capture
 */
static esp_err_t thermal_start_handler(httpd_req_t *req)
{
    uint8_t regB1 = Acces_Read_Reg(0xB1);
    regB1 |= (B1_START_CAPTURE | B1_SINGLE_CONT);
    Acces_Write_Reg(0xB1, regB1);
    ESP_LOGI(THERMAL_WEB_TAG, "Web start capture: reg 0xB1=0x%02X", regB1);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":1}");
    return ESP_OK;
}

/**
 * @brief Handler for POST /thermal/stop - stops sensor capture
 */
static esp_err_t thermal_stop_handler(httpd_req_t *req)
{
    uint8_t regB1 = Acces_Read_Reg(0xB1);
    regB1 &= ~(B1_START_CAPTURE | B1_SINGLE_CONT);
    Acces_Write_Reg(0xB1, regB1);
    ESP_LOGI(THERMAL_WEB_TAG, "Web stop capture: reg 0xB1=0x%02X", regB1);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":1}");
    return ESP_OK;
}

/**
 * @brief Handler for GET /thermal/frame - returns latest frame as binary
 *
 * Response: application/octet-stream
 * Body: uint16_t[5040] raw thermal data in little-endian byte order
 *       (5040 = 80 cols × 63 rows)
 */
static esp_err_t thermal_frame_get_handler(httpd_req_t *req);

/**
 * @brief Handler for GET /thermal/debug - returns diagnostic JSON
 */
static esp_err_t thermal_debug_get_handler(httpd_req_t *req)
{
    char buf[512];
    /* Get latest frame via mutex-protected snapshot */
    uint32_t frameNum = 0;
    int hasFrame = senxorTask_GetLatestFrame(g_FrameBuf, 5040, &frameNum) ? 1 : 0;

    /* Metadata: mFrame[0]=frame#, [1]=VDD, [2]=dieTemp, [5]=maxTemp, [6]=minTemp */
    uint16_t metaFrameNum = hasFrame ? g_FrameBuf[0] : 0;
    uint16_t metaVDD      = hasFrame ? g_FrameBuf[1] : 0;
    uint16_t metaDieTemp  = hasFrame ? g_FrameBuf[2] : 0;
    uint16_t metaMaxTemp  = hasFrame ? g_FrameBuf[5] : 0;
    uint16_t metaMinTemp  = hasFrame ? g_FrameBuf[6] : 0;
    uint16_t firstPx      = hasFrame ? g_FrameBuf[THERMAL_IMAGE_OFFSET] : 0;

    /* Read temperature format registers */
    uint8_t regB9 = Acces_Read_Reg(0xB9);
    uint8_t reg31 = Customer_Interface_Read_Registers(0x31);
    uint8_t tempRes = (regB9 & 0x80) ? 100 : 10;

    snprintf(buf, sizeof(buf),
        "{\"req\":%lu,\"sent\":%lu,\"empty\":%lu,\"hasFrame\":%d,\"frameNum\":%lu,"
        "\"vdd_mV\":%u,\"dieTemp\":%u,"
        "\"maxTemp\":%u,\"minTemp\":%u,\"firstPx\":%u,"
        "\"regB9\":\"0x%02X\",\"tempRes\":%u,\"tempFmt\":%u,"
        "\"regB1\":\"0x%02X\"}",
        g_frame_req_count, g_frame_sent_count, g_frame_empty_count,
        hasFrame, frameNum,
        metaVDD, metaDieTemp,
        metaMaxTemp, metaMinTemp, firstPx,
        regB9, tempRes, reg31,
        Acces_Read_Reg(0xB1));

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_sendstr(req, buf);
    return ESP_OK;
}
static esp_err_t thermal_frame_get_handler(httpd_req_t *req)
{
    g_frame_req_count++;

    /* Use mutex-protected snapshot (separate from TCP queue) */
    uint32_t frameNum = 0;
    if (!senxorTask_GetLatestFrame(g_FrameBuf, 5040, &frameNum))
    {
        g_frame_empty_count++;
        httpd_resp_set_type(req, "application/octet-stream");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    g_frame_sent_count++;

    /* Read temperature format: tempRes (10 or 100), tempFmt (0=Kelvin, 1=Celsius, 2=Fahrenheit) */
    uint8_t regB9 = Acces_Read_Reg(0xB9);
    uint8_t tempRes = (regB9 & 0x80) ? 100 : 10;
    uint8_t tempFmt = Customer_Interface_Read_Registers(0x31);

    char hdrVal[32];
    snprintf(hdrVal, sizeof(hdrVal), "%u,%u", tempRes, tempFmt);

    httpd_resp_set_type(req, "application/octet-stream");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "X-Temp-Res", hdrVal);

    /* Send only the image portion (skip 80-word metadata header) */
    httpd_resp_send(req, (const char *)&g_FrameBuf[THERMAL_IMAGE_OFFSET], THERMAL_IMAGE_SIZE * sizeof(uint16_t));

    return ESP_OK;
}

/* ────────────────────────────────────────────────────────────────
 * Public API
 * ──────────────────────────────────────────────────────────────── */

void thermal_web_init(httpd_handle_t server)
{
    if (server == NULL) {
        ESP_LOGE(THERMAL_WEB_TAG, "HTTP server handle is NULL, cannot register thermal web handlers");
        return;
    }

    /* Register URI handler: serve the HTML page */
    httpd_uri_t page_uri = {
        .uri       = "/thermal",
        .method    = HTTP_GET,
        .handler   = thermal_page_get_handler,
        .user_ctx  = NULL
    };
    esp_err_t err = httpd_register_uri_handler(server, &page_uri);
    if (err == ESP_OK) {
        ESP_LOGI(THERMAL_WEB_TAG, "Registered /thermal page handler");
    } else {
        ESP_LOGE(THERMAL_WEB_TAG, "Failed to register /thermal: %s", esp_err_to_name(err));
    }

    /* Register URI handler: serve the raw temperature matrix page */
    httpd_uri_t raw_uri = {
        .uri       = "/thermal/raw",
        .method    = HTTP_GET,
        .handler   = thermal_raw_get_handler,
        .user_ctx  = NULL
    };
    err = httpd_register_uri_handler(server, &raw_uri);
    if (err == ESP_OK) {
        ESP_LOGI(THERMAL_WEB_TAG, "Registered /thermal/raw page handler");
    } else {
        ESP_LOGW(THERMAL_WEB_TAG, "Failed to register /thermal/raw: %s", esp_err_to_name(err));
    }

    /* Register URI handler: start capture (web Connect button) */
    httpd_uri_t start_uri = {
        .uri       = "/thermal/start",
        .method    = HTTP_POST,
        .handler   = thermal_start_handler,
        .user_ctx  = NULL
    };
    err = httpd_register_uri_handler(server, &start_uri);
    if (err == ESP_OK) {
        ESP_LOGI(THERMAL_WEB_TAG, "Registered /thermal/start handler");
    } else {
        ESP_LOGW(THERMAL_WEB_TAG, "Failed to register /thermal/start: %s", esp_err_to_name(err));
    }

    /* Register URI handler: stop capture (web Disconnect button) */
    httpd_uri_t stop_uri = {
        .uri       = "/thermal/stop",
        .method    = HTTP_POST,
        .handler   = thermal_stop_handler,
        .user_ctx  = NULL
    };
    err = httpd_register_uri_handler(server, &stop_uri);
    if (err == ESP_OK) {
        ESP_LOGI(THERMAL_WEB_TAG, "Registered /thermal/stop handler");
    } else {
        ESP_LOGW(THERMAL_WEB_TAG, "Failed to register /thermal/stop: %s", esp_err_to_name(err));
    }

    /* Register URI handler: serve the raw frame data */
    httpd_uri_t frame_uri = {
        .uri       = "/thermal/frame",
        .method    = HTTP_GET,
        .handler   = thermal_frame_get_handler,
        .user_ctx  = NULL
    };
    err = httpd_register_uri_handler(server, &frame_uri);
    if (err == ESP_OK) {
        ESP_LOGI(THERMAL_WEB_TAG, "Registered /thermal/frame handler");
    } else {
        ESP_LOGE(THERMAL_WEB_TAG, "Failed to register /thermal/frame: %s", esp_err_to_name(err));
    }

    /* Register URI handler: debug info */
    httpd_uri_t debug_uri = {
        .uri       = "/thermal/debug",
        .method    = HTTP_GET,
        .handler   = thermal_debug_get_handler,
        .user_ctx  = NULL
    };
    err = httpd_register_uri_handler(server, &debug_uri);
    if (err == ESP_OK) {
        ESP_LOGI(THERMAL_WEB_TAG, "Registered /thermal/debug handler");
    } else {
        ESP_LOGE(THERMAL_WEB_TAG, "Failed to register /thermal/debug: %s", esp_err_to_name(err));
    }

    ESP_LOGI(THERMAL_WEB_TAG, "Thermal web viewer ready - open http://<IP>/thermal in a browser");
}
