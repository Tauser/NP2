"""Exercise actual FlashCoordinator quota I/O against fault-injected NVS."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "firmware/build/quota-host-tests"
OUT.mkdir(parents=True, exist_ok=True)
source = (ROOT / "firmware/main/flash_coordinator.c").read_text(encoding="utf-8")

def function(signature):
    start = source.index(signature)
    begin = source.index("{", start)
    depth, end = 1, begin + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]

shim = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "coingecko_quota.h"
typedef int esp_err_t;
typedef int nvs_handle_t;
enum { ESP_OK, ESP_ERR_INVALID_ARG, ESP_ERR_INVALID_STATE, ESP_ERR_NOT_FOUND,
       ESP_ERR_NVS_NOT_FOUND, ESP_ERR_INVALID_CRC, ESP_ERR_INVALID_SIZE, ESP_FAIL };
#define NVS_PARTITION "nvs"
#define NVS_READONLY 0
#define NVS_READWRITE 1
#define ESP_LOGI(...) ((void)0)
typedef struct { bool ready; } flash_coordinator_status_t;
static bool ready=true, exists=false, fail_set=false, fail_commit=false, corrupt_verify=false;
static coingecko_quota_record_t disk, staged;
static void flash_coordinator_get_status(flash_coordinator_status_t *out) { out->ready=ready; }
static esp_err_t nvs_open_from_partition(const char *part, const char *ns, int mode, nvs_handle_t *out)
{ assert(strcmp(part,"nvs")==0 && strcmp(ns,"cg_quota")==0); *out=1;
  return mode==NVS_READONLY && !exists ? ESP_ERR_NVS_NOT_FOUND : ESP_OK; }
static void nvs_close(nvs_handle_t handle) { assert(handle==1); }
static esp_err_t nvs_get_blob(nvs_handle_t handle, const char *key, void *out, size_t *size)
{ assert(handle==1 && strcmp(key,"budget_v1")==0); if (!exists) return ESP_ERR_NVS_NOT_FOUND;
  assert(*size>=sizeof(disk)); *size=sizeof(disk); memcpy(out,&disk,sizeof(disk)); return ESP_OK; }
static esp_err_t nvs_set_blob(nvs_handle_t handle, const char *key, const void *in, size_t size)
{ assert(handle==1 && strcmp(key,"budget_v1")==0 && size==sizeof(staged));
  if (fail_set) return ESP_FAIL;
  memcpy(&staged,in,size); return ESP_OK; }
static esp_err_t nvs_commit(nvs_handle_t handle)
{ assert(handle==1); if (fail_commit) return ESP_FAIL; disk=staged; exists=true;
  if (corrupt_verify) disk.crc32^=1U;
  return ESP_OK; }
'''
tests = r'''
int main(void)
{
    coingecko_quota_t quota;
    coingecko_quota_record_t proposal, read;
    assert(flash_coordinator_get_coingecko_quota(&read)==ESP_ERR_NOT_FOUND);
    assert(coingecko_quota_init(&quota,NULL));
    assert(coingecko_quota_prepare(&quota,202610U,1791400000U,1LL,&proposal)==COINGECKO_QUOTA_RESERVE);
    fail_set=true;
    assert(write_coingecko_quota((const uint8_t *)&proposal,sizeof(proposal))==ESP_FAIL);
    assert(!exists);
    fail_set=false; fail_commit=true;
    assert(write_coingecko_quota((const uint8_t *)&proposal,sizeof(proposal))==ESP_FAIL);
    assert(!exists);
    fail_commit=false;
    assert(write_coingecko_quota((const uint8_t *)&proposal,sizeof(proposal))==ESP_OK);
    assert(flash_coordinator_get_coingecko_quota(&read)==ESP_OK);
    assert(memcmp(&proposal,&read,sizeof(read))==0);
    assert(write_coingecko_quota((const uint8_t *)&proposal,sizeof(proposal))==ESP_ERR_INVALID_STATE);
    assert(coingecko_quota_init(&quota,&read));
    assert(coingecko_quota_prepare(&quota,202610U,read.restart_after_s,1LL,&proposal)==COINGECKO_QUOTA_WAIT);
    assert(coingecko_quota_prepare(&quota,202610U,read.restart_after_s,1200000001LL,&proposal)==COINGECKO_QUOTA_RESERVE);
    corrupt_verify=true;
    assert(write_coingecko_quota((const uint8_t *)&proposal,sizeof(proposal))==ESP_ERR_INVALID_CRC);
    assert(flash_coordinator_get_coingecko_quota(&read)==ESP_ERR_INVALID_CRC);
    assert(write_coingecko_quota((const uint8_t *)&proposal,sizeof(proposal))==ESP_ERR_INVALID_CRC);
    ready=false; assert(flash_coordinator_get_coingecko_quota(&read)==ESP_ERR_INVALID_STATE);
    assert(write_coingecko_quota((const uint8_t *)&proposal,0U)==ESP_ERR_INVALID_SIZE);
    assert(flash_coordinator_get_coingecko_quota(NULL)==ESP_ERR_INVALID_ARG);
    puts("Real quota NVS: missing record, set/commit failure, readback CRC, stale generation and corruption passed");
    return 0;
}
'''
generated = OUT / "coingecko_storage_test.c"
generated.write_text(shim + function("esp_err_t flash_coordinator_get_coingecko_quota(") +
                     "\n" + function("static esp_err_t write_coingecko_quota(") + tests,
                     encoding="utf-8")
exe = OUT / "coingecko_storage_test.exe"
subprocess.run(["gcc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-pedantic",
                "-I", str(ROOT / "firmware/main"), str(generated),
                str(ROOT / "firmware/main/coingecko_quota.c"),
                str(ROOT / "firmware/main/cache_record.c"), "-o", str(exe)], check=True)
subprocess.run([str(exe)], check=True)
