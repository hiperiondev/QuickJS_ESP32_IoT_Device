#include <Arduino.h>
#include "quickjs.h"
#include "module_type.h"
#include "module_utils.h"
#include <IRsend.h>
#include <IRrecv.h>

#define IR_DEFAULT_HZ 38

static IRsend *g_irsend = NULL;
static IRrecv *g_irrecv = NULL;
static bool g_is_recving = false;
static decode_results results;

static JSValue esp32_ir_sendBegin(JSContext *ctx, JSValueConst jsThis, int argc,
                               JSValueConst *argv)
{
  uint32_t pin;
  JS_ToUint32(ctx, &pin, argv[0]);

  if( g_irsend != NULL ){
    delete g_irsend;
    g_irsend = NULL;
  }

  g_irsend = new IRsend(pin);
  g_irsend->begin();

  return JS_UNDEFINED;
}

static JSValue esp32_ir_send(JSContext *ctx, JSValueConst jsThis, int argc,
                               JSValueConst *argv)
{
  if( g_irsend == NULL )
    return JS_EXCEPTION;

  uint32_t data;
  JS_ToUint32(ctx, &data, argv[0]);
  uint32_t repeat = 0;
  if( argc >= 2 )
    JS_ToUint32(ctx, &repeat, argv[1]);

  g_irsend->sendNEC(data, 32, repeat);

  return JS_UNDEFINED;
}

static JSValue esp32_ir_sendRaw(JSContext *ctx, JSValueConst jsThis, int argc,
                               JSValueConst *argv)
{
  if( g_irsend == NULL )
    return JS_EXCEPTION;

  uint16_t *p_buffer;
  uint8_t unit_size;
  uint32_t unit_num;
  JSValue vbuffer = getArrayBuffer(ctx, argv[0], (void**)&p_buffer, &unit_size, &unit_num);
  if( JS_IsNull(vbuffer) )
    return JS_EXCEPTION;
  if( unit_size != 2 ){
    JS_FreeValue(ctx, vbuffer);
    return JS_EXCEPTION;
  }

  g_irsend->sendRaw(p_buffer, unit_num, IR_DEFAULT_HZ);

  JS_FreeValue(ctx, vbuffer);

  return JS_UNDEFINED;
}

static JSValue esp32_ir_recvBegin(JSContext *ctx, JSValueConst jsThis, int argc,
                               JSValueConst *argv)
{
  uint32_t pin;
  JS_ToUint32(ctx, &pin, argv[0]);

  if( g_irrecv != NULL ){
    g_irrecv->disableIRIn();
    g_is_recving = false;
    delete g_irrecv;
    g_irrecv = NULL;
  }

  g_irrecv = new IRrecv(pin, kRawBuf, kTimeoutMs, true);

  return JS_UNDEFINED;
}

static JSValue esp32_ir_recvStart(JSContext *ctx, JSValueConst jsThis, int argc,
                               JSValueConst *argv)
{
  if( g_irrecv == NULL )
    return JS_EXCEPTION;
  
  g_irrecv->enableIRIn();
  g_irrecv->resume();
  g_is_recving = true;

  return JS_UNDEFINED;
}

static JSValue esp32_ir_recvStop(JSContext *ctx, JSValueConst jsThis, int argc,
                               JSValueConst *argv)
{
  if( g_irrecv == NULL )
    return JS_EXCEPTION;

  g_irrecv->disableIRIn();
  g_is_recving = false;

  return JS_UNDEFINED;
}

static JSValue esp32_ir_checkRecv(JSContext *ctx, JSValueConst jsThis, int argc,
                               JSValueConst *argv)
{
  if( g_irrecv == NULL )
    return JS_EXCEPTION;

  if( g_irrecv->decode(&results) ){
    g_irrecv->resume();
    if( results.decode_type != NEC_LIKE )
      return JS_NewUint32(ctx, 0);

    return JS_NewUint32(ctx, results.value);
  }

  return JS_NewUint32(ctx, 0);
}

static const JSCFunctionListEntry ir_funcs[] = {
    JSCFunctionListEntry{"sendBegin", 0, JS_DEF_CFUNC, 0, {
                           func : {1, JS_CFUNC_generic, esp32_ir_sendBegin}
                         }},
    JSCFunctionListEntry{"send", 0, JS_DEF_CFUNC, 0, {
                           func : {2, JS_CFUNC_generic, esp32_ir_send}
                         }},
    JSCFunctionListEntry{"sendRaw", 0, JS_DEF_CFUNC, 0, {
                           func : {1, JS_CFUNC_generic, esp32_ir_sendRaw}
                         }},
    JSCFunctionListEntry{"recvBegin", 0, JS_DEF_CFUNC, 0, {
                           func : {1, JS_CFUNC_generic, esp32_ir_recvBegin}
                         }},
    JSCFunctionListEntry{"recvStart", 0, JS_DEF_CFUNC, 0, {
                           func : {0, JS_CFUNC_generic, esp32_ir_recvStart}
                         }},
    JSCFunctionListEntry{"recvStop", 0, JS_DEF_CFUNC, 0, {
                           func : {0, JS_CFUNC_generic, esp32_ir_recvStop}
                         }},
    JSCFunctionListEntry{"checkRecv", 0, JS_DEF_CFUNC, 0, {
                           func : {0, JS_CFUNC_generic, esp32_ir_checkRecv}
                         }},
};

JSModuleDef *addModule_ir(JSContext *ctx, JSValue global)
{
  JSModuleDef *mod;

  mod = JS_NewCModule(ctx, "Ir", [](JSContext *ctx, JSModuleDef *m)
                      { return JS_SetModuleExportList(
                            ctx, m, ir_funcs,
                            sizeof(ir_funcs) / sizeof(JSCFunctionListEntry)); });
  if (mod){
    JS_AddModuleExportList(
        ctx, mod, ir_funcs,
        sizeof(ir_funcs) / sizeof(JSCFunctionListEntry));
  }

  return mod;
}

void endModule_ir(void){
  if( g_irsend != NULL ){
    delete g_irsend;
    g_irsend = NULL;
  }
  if( g_irrecv != NULL ){
    g_irrecv->disableIRIn();
    g_is_recving = false;
    delete g_irrecv;
    g_irrecv = NULL;
  }
}

JsModuleEntry ir_module = {
  NULL,
  addModule_ir,
  NULL,
  endModule_ir
};
