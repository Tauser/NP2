#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "lfs.h"
#include "cache_record.h"
#include "offline_data_codec.h"

typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_INVALID_SIZE 0x104
#define ESP_ERR_NOT_FOUND 0x105
#define ESP_ERR_INVALID_RESPONSE 0x108
#define ESP_ERR_INVALID_CRC 0x109
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_TIMEOUT 0x107
#define ESP_LOGE(tag, ...) do { (void)(tag); printf(__VA_ARGS__); puts(""); } while (0)
#define ESP_LOGI ESP_LOGE
#define ESP_LOGW ESP_LOGE
#define pdMS_TO_TICKS(ms) (ms)
static void vTaskDelay(unsigned ticks) { (void)ticks; }
static bool inject_deadline;
static int64_t fake_clock;
static int64_t esp_timer_get_time(void)
{ if (inject_deadline) fake_clock += 100000000LL; return fake_clock; }
static const char *esp_err_to_name(int result) { return result == ESP_OK ? "ESP_OK" : "error"; }

static uint8_t disk[9U * 1024U * 1024U];
static lfs_t filesystem;
static lfs_file_t handles[4];
static bool opened[4];
static unsigned proposals;
static bool inject_proposal_io, inject_cleanup_io, inject_sync_io;
static int block_read(const struct lfs_config *c, lfs_block_t b, lfs_off_t o, void *p, lfs_size_t n)
{ memcpy(p, disk + b * c->block_size + o, n); return 0; }
static int block_prog(const struct lfs_config *c, lfs_block_t b, lfs_off_t o, const void *p, lfs_size_t n)
{ uint8_t *d = disk + b * c->block_size + o; const uint8_t *s = p;
  for (lfs_size_t i=0; i<n; ++i) { assert((d[i] & s[i]) == s[i]); d[i] &= s[i]; } return 0; }
static int block_erase(const struct lfs_config *c, lfs_block_t b)
{ memset(disk + b * c->block_size, 255, c->block_size); return 0; }
static int block_sync(const struct lfs_config *c) { (void)c; return 0; }
static struct lfs_config config = {
    .read=block_read, .prog=block_prog, .erase=block_erase, .sync=block_sync,
    .read_size=128, .prog_size=128, .block_size=4096,
    .block_count=sizeof(disk)/4096, .block_cycles=512, .cache_size=512, .lookahead_size=128
};
static int posix_result(int result)
{
    if (result >= 0) return result;
    errno = result == LFS_ERR_NOSPC ? ENOSPC : (result == LFS_ERR_NOENT ? ENOENT : EIO);
    return -1;
}
static int test_open(const char *path, int flags, ...)
{
    if (strstr(path, "proposal.") && (flags & O_CREAT)) {
        ++proposals;
        if (inject_proposal_io) { errno=EIO; return -1; }
    }
    int lf = (flags & O_RDWR) ? LFS_O_RDWR : ((flags & O_WRONLY) ? LFS_O_WRONLY : LFS_O_RDONLY);
    if (flags & O_CREAT) lf |= LFS_O_CREAT;
    if (flags & O_TRUNC) lf |= LFS_O_TRUNC;
    if (flags & O_APPEND) lf |= LFS_O_APPEND;
    for (int i=0; i<4; ++i) if (!opened[i]) {
        int r=posix_result(lfs_file_open(&filesystem, &handles[i], path, lf));
        if (r<0) return r;
        opened[i]=true; return i;
    }
    errno=EMFILE; return -1;
}
static ssize_t test_write(int fd, const void *p, size_t n)
{ return posix_result(lfs_file_write(&filesystem, &handles[fd], p, (lfs_size_t)n)); }
static ssize_t test_read(int fd, void *p, size_t n)
{ return posix_result(lfs_file_read(&filesystem, &handles[fd], p, (lfs_size_t)n)); }
static off_t test_lseek(int fd, off_t offset, int whence)
{ return posix_result(lfs_file_seek(&filesystem, &handles[fd], offset, whence)); }
static int test_fsync(int fd)
{ if (inject_sync_io) { inject_sync_io=false; errno=EIO; return -1; }
  return posix_result(lfs_file_sync(&filesystem, &handles[fd])); }
static int test_close(int fd)
{ opened[fd]=false; return posix_result(lfs_file_close(&filesystem, &handles[fd])); }
static int test_unlink(const char *path)
{ if (inject_cleanup_io && proposals && strstr(path, "full-probe.tmp")) { errno=EIO; return -1; }
  return posix_result(lfs_remove(&filesystem, path)); }
static esp_err_t esp_littlefs_info(const char *part, size_t *total, size_t *used)
{ (void)part; lfs_ssize_t blocks=lfs_fs_size(&filesystem); if (blocks<0) return ESP_FAIL;
  *total=sizeof(disk); *used=(size_t)blocks*4096U; return ESP_OK; }
static void set_full_probe_progress(bool active, bool syncing, uint32_t written, uint32_t target)
{ (void)active; (void)syncing; (void)written; (void)target; }
static void refresh_cache_status(void) {}
static esp_err_t select_latest_cache(cache_record_header_t *out);
#define open test_open
#define write test_write
#define read test_read
#define lseek test_lseek
#define fsync test_fsync
#define close test_close
#define unlink test_unlink
#include "probe_under_test.inc"

static esp_err_t select_latest_cache(cache_record_header_t *out)
{
    cache_record_header_t a={0}, b={0};
    bool av=validate_cache_generation(LITTLEFS_GENERATION_ZERO_PATH,&a)==ESP_OK;
    bool bv=validate_cache_generation(LITTLEFS_GENERATION_ONE_PATH,&b)==ESP_OK;
    return cache_record_select_newest(&a,av,&b,bv,out) ? ESP_OK : ESP_ERR_NOT_FOUND;
}
static void assert_only_cache_files(void)
{
    lfs_dir_t directory; struct lfs_info info;
    assert(lfs_dir_open(&filesystem,&directory,"/lfsdiag")==0);
    unsigned count=0; int r;
    while ((r=lfs_dir_read(&filesystem,&directory,&info))>0) {
        if (strcmp(info.name,".")==0 || strcmp(info.name,"..")==0) continue;
        assert(strcmp(info.name,"cache.0")==0 || strcmp(info.name,"cache.1")==0);
        ++count;
    }
    assert(r==0 && count==2); assert(lfs_dir_close(&filesystem,&directory)==0);
}
static void setup(size_t payload_size)
{
    memset(disk,255,sizeof(disk)); memset(opened,0,sizeof(opened));
    inject_proposal_io=false; inject_cleanup_io=false; inject_sync_io=false; proposals=0;
    inject_deadline=false; fake_clock=0;
    assert(lfs_format(&filesystem,&config)==0); assert(lfs_mount(&filesystem,&config)==0);
    assert(lfs_mkdir(&filesystem,"/lfsdiag")==0);
    uint8_t payload[4096]; memset(payload,42,sizeof(payload));
    if (payload_size == OFFLINE_DATA_ENCODED_SIZE) {
        offline_data_snapshot_t snapshot={.schema_version=1,.origin=OFFLINE_DATA_ORIGIN_CACHE,
            .weather={.available=true,.temperature_deci_c=230,.relative_humidity_percent=50,
                      .observed_at_unix_s=1760000000}};
        assert(offline_data_snapshot_encode(&snapshot,payload,payload_size));
    }
    assert(write_cache_record_at_path(LITTLEFS_GENERATION_ZERO_PATH,2,payload,payload_size,NULL)==ESP_OK);
    assert(write_cache_record_at_path(LITTLEFS_GENERATION_ONE_PATH,3,payload,payload_size,NULL)==ESP_OK);
}
int main(void)
{
    setvbuf(stdout,NULL,_IONBF,0);
    setup(4096);
    int r=run_cache_full_probe(1);
    printf("normal: result=%d proposals=%u\n",r,proposals);
    assert(r==ESP_OK); assert_only_cache_files();
    cache_record_header_t h; assert(select_latest_cache(&h)==ESP_OK && h.generation==3);
    r=run_cache_full_probe(2); printf("repeat: result=%d\n",r); assert(r==ESP_OK);
    assert_only_cache_files(); assert(lfs_unmount(&filesystem)==0);
    setup(OFFLINE_DATA_ENCODED_SIZE); r=run_cache_full_probe(6);
    printf("domain snapshot: result=%d\n",r); assert(r==ESP_OK); assert_only_cache_files();
    assert(lfs_unmount(&filesystem)==0);
    assert(lfs_mount(&filesystem,&config)==0);
    assert(select_latest_cache(&h)==ESP_OK && h.generation==3); assert_only_cache_files();
    assert(lfs_unmount(&filesystem)==0);
    setup(4096); inject_proposal_io=true; r=run_cache_full_probe(3);
    printf("proposal EIO: result=%d\n",r); assert(r!=ESP_OK); assert_only_cache_files();
    assert(lfs_unmount(&filesystem)==0);
    setup(4096); inject_cleanup_io=true; r=run_cache_full_probe(4);
    printf("cleanup EIO: result=%d\n",r); assert(r!=ESP_OK);
    inject_cleanup_io=false; assert(remove_full_probe_files()==ESP_OK); assert_only_cache_files();
    assert(lfs_unmount(&filesystem)==0);
    setup(4096); inject_sync_io=true; r=run_cache_full_probe(5);
    printf("sync EIO: result=%d\n",r); assert(r!=ESP_OK); assert_only_cache_files();
    assert(lfs_unmount(&filesystem)==0);
    setup(4096); inject_deadline=true; r=run_cache_full_probe(7);
    printf("deadline: result=%d\n",r); assert(r==ESP_ERR_TIMEOUT); assert_only_cache_files();
    assert(lfs_unmount(&filesystem)==0);
    puts("cache full probe host tests PASS"); return 0;
}
