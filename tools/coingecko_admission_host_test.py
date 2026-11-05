"""Compile the actual HTTPS admission functions with bounded fake I/O."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "firmware/build/quota-host-tests"
OUT.mkdir(parents=True, exist_ok=True)
source = (ROOT / "firmware/main/network_validation_service.c").read_text(encoding="utf-8")

def function(name):
    start = source.index("static esp_err_t " + name + "(")
    begin = source.index("{", start)
    depth = 1
    end = begin + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]

shim = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "coingecko_quota.h"
typedef int esp_err_t;
enum { ESP_OK, ESP_ERR_INVALID_STATE, ESP_ERR_NOT_FOUND, ESP_ERR_NOT_FINISHED, ESP_FAIL };
#define NP2_VALID_EPOCH_SECONDS 1735689600LL
#define FLASH_COORDINATOR_COINGECKO_API_KEY_BYTES 64U
#define HTTP_METHOD_GET 0
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
typedef struct { bool trusted; } time_service_status_t;
static coingecko_quota_t s_coingecko_quota;
static bool s_coingecko_quota_loaded;
static bool trusted, exists, fail_save, fail_key, corrupt_read, uncertain_ack;
static int calls, saves, http_result;
static int64_t mono;
static time_t epoch;
static coingecko_quota_record_t disk;
static time_t fake_time(time_t *out) { if (out) *out = epoch; return epoch; }
#define time fake_time
static struct tm *gmtime_r(const time_t *value, struct tm *out)
{ (void)value; memset(out, 0, sizeof(*out)); out->tm_year = 126; out->tm_mon = 9; return out; }
static int64_t esp_timer_get_time(void) { return mono; }
static void time_service_get_status(time_service_status_t *out) { out->trusted = trusted; }
static esp_err_t flash_coordinator_get_coingecko_quota(coingecko_quota_record_t *out)
{ if (corrupt_read) return ESP_FAIL; if (!exists) return ESP_ERR_NOT_FOUND; *out = disk; return ESP_OK; }
static esp_err_t flash_coordinator_reserve_coingecko_quota(const coingecko_quota_record_t *in)
{ ++saves; if (fail_save) return ESP_FAIL; disk = *in; exists = true; return uncertain_ack ? ESP_FAIL : ESP_OK; }
static esp_err_t flash_coordinator_copy_coingecko_api_key(char *out, size_t size)
{ if (fail_key) return ESP_FAIL; assert(size >= 5U); memcpy(out, "test", 5U); return ESP_OK; }
static void secure_zero(void *out, size_t size) { memset(out, 0, size); }
static const char *esp_err_to_name(int error) { (void)error; return "mock"; }
static uint32_t remaining_phase_budget_ms(int64_t start, uint32_t budget)
{ return mono - start >= (int64_t)budget * 1000LL ? 0U : budget - (uint32_t)((mono-start)/1000LL); }
static esp_err_t perform_https_request_with_header(const char *url, uint32_t timeout,
    int method, const char *name, const char *key, size_t max, uint8_t *out, size_t size, size_t *length)
{ (void)url; (void)timeout; (void)method; (void)max; (void)out; (void)size; (void)length;
  assert(strcmp(name, "x-cg-demo-api-key") == 0); assert(strcmp(key, "test") == 0);
  assert(exists && disk.reserved >= 8011U); ++calls; return http_result; }
static void reset(void)
{ memset(&s_coingecko_quota,0,sizeof(s_coingecko_quota)); s_coingecko_quota_loaded=false;
  trusted=true; exists=fail_save=fail_key=corrupt_read=uncertain_ack=false;
  calls=saves=0; http_result=ESP_OK; mono=1LL; epoch=1791400000; }
'''
tests = r'''
static esp_err_t request(void)
{ uint8_t body[8]; size_t size=0U; return perform_coingecko_request("mock",20000U,sizeof(body),body,sizeof(body),&size); }
int main(void)
{
    reset(); assert(request()==ESP_OK); assert(calls==1 && saves==1 && disk.reserved==8011U);
    assert(request()==ESP_ERR_NOT_FINISHED); assert(calls==1 && saves==1);
    mono += 300LL*1000000LL; epoch += 300;
    assert(request()==ESP_ERR_NOT_FINISHED); assert(calls==1);
    mono += 900LL*1000000LL; epoch += 900; http_result=ESP_FAIL;
    assert(request()==ESP_FAIL); assert(calls==2 && saves==2 && s_coingecko_quota.remaining==0U);
    /* Recovered storage cannot reuse a credit or skip a full interval. */
    s_coingecko_quota_loaded=false;
    assert(request()==ESP_ERR_NOT_FINISHED); assert(calls==2 && saves==2);
    epoch=disk.restart_after_s; mono += 10000LL*1000000LL; http_result=ESP_OK;
    assert(request()==ESP_OK); assert(calls==3 && saves==3 && disk.reserved==8013U);
    reset(); fail_save=true; assert(request()==ESP_FAIL); assert(calls==0 && saves==1);
    reset(); corrupt_read=true; assert(request()==ESP_FAIL); assert(calls==0 && saves==0);
    reset(); trusted=false; assert(request()==ESP_ERR_INVALID_STATE); assert(calls==0 && saves==0);
    reset(); fail_key=true; assert(request()==ESP_FAIL); assert(calls==0 && saves==0);
    /* ACK timeout after successful commit must not grant its block on retry. */
    reset(); uncertain_ack=true; assert(request()==ESP_FAIL); assert(calls==0 && saves==1);
    uncertain_ack=false; assert(request()==ESP_ERR_NOT_FINISHED); assert(calls==0 && saves==1);
    /* Reload late in uptime must still wait from the reload, not from boot. */
    reset(); mono=3600LL*1000000LL; assert(request()==ESP_OK);
    s_coingecko_quota_loaded=false; ++mono; ++epoch;
    assert(request()==ESP_ERR_NOT_FINISHED); assert(calls==1 && saves==1);
    mono+=1199LL*1000000LL; epoch+=50000;
    assert(request()==ESP_ERR_NOT_FINISHED); assert(calls==1 && saves==1);
    mono+=1000000LL; assert(request()==ESP_OK);
    assert(calls==2 && saves==2 && disk.reserved==8012U);
    puts("Real CoinGecko admission: durable-before-HTTP, manual throttling, failures, reboot and uncertain ACK passed");
    return 0;
}
'''
# Logs compile away; their mock formatter need not be instantiated.
shim = shim.replace('static const char *esp_err_to_name(int error) { (void)error; return "mock"; }', '')
generated = OUT / "coingecko_admission_test.c"
generated.write_text(shim + function("admit_coingecko_request") + "\n" +
                     function("perform_coingecko_request") + tests, encoding="utf-8")
exe = OUT / "coingecko_admission_test.exe"
subprocess.run(["gcc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-pedantic",
                "-I", str(ROOT / "firmware/main"), str(generated),
                str(ROOT / "firmware/main/coingecko_quota.c"),
                str(ROOT / "firmware/main/cache_record.c"), "-o", str(exe)], check=True)
subprocess.run([str(exe)], check=True)
