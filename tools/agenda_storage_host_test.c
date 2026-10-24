#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <io.h>
#include "esp_heap_caps.h"
static const char *directory;
static bool fail_sync,fail_rename;
static void path(const char *input,char *out,size_t n)
{ const char *name=strrchr(input,'/'); snprintf(out,n,"%s/%s",directory,name == NULL ? input : name+1); }
static FILE *mapped_open(const char *name,const char *mode)
{ char resolved[1024]; path(name,resolved,sizeof(resolved)); return fopen(resolved,mode); }
static int mapped_rename(const char *old,const char *new_name)
{
    if (fail_rename) return -1;
    char from[1024],to[1024]; path(old,from,sizeof(from)); path(new_name,to,sizeof(to));
    /* LittleFS/POSIX rename replaces a slot atomically. Windows C rename
     * doesn't; emulate only that platform difference in this I/O shim. */
    remove(to); return rename(from,to);
}
static int mapped_sync(int fd) { return fail_sync ? -1 : _commit(fd); }
void *heap_caps_calloc(size_t n,size_t size,unsigned caps) { (void)caps; return calloc(n,size); }
#define fopen mapped_open
#define rename mapped_rename
#define fsync mapped_sync
#include "../firmware/main/agenda_storage.c"
#undef fopen
#undef rename
#undef fsync
static void reboot(void) { free(s_workspace); s_workspace=NULL; s_result=ESP_ERR_INVALID_STATE; }
static void cleanup(void)
{
    const char *files[]={"agenda.0","agenda.1","agenda.tmp"}; char file[1024];
    for (unsigned i=0;i<3;++i) { path(files[i],file,sizeof(file)); remove(file); }
}
int main(int argc,char **argv)
{
    assert(argc == 2); directory=argv[1]; cleanup();
    agenda_data_t data,loaded; agenda_init(&data);
    assert(agenda_storage_owner_load() == ESP_ERR_NOT_FOUND);
    assert(agenda_storage_owner_format_allowed());
    agenda_command_t c={.request_id=1,.action=AGENDA_UPSERT,.item={.kind=AGENDA_TASK,.reminder_minutes=AGENDA_REMINDER_NONE}};
    strcpy(c.item.title,"Primeira geração"); assert(agenda_apply(&data,&c) == AGENDA_OK);
    esp_err_t first_write=agenda_storage_owner_write(&data);
    if (first_write != ESP_OK) fprintf(stderr,"first write result=%d errno=%d\n",first_write,errno);
    assert(first_write == ESP_OK);
    assert(!agenda_storage_owner_format_allowed());
    reboot(); assert(agenda_storage_owner_load() == ESP_OK && agenda_storage_copy(&loaded) == ESP_OK);
    assert(!memcmp(&loaded,&data,sizeof(data)));
    c.item=data.items[0]; strcpy(c.item.title,"Segunda geração"); assert(agenda_apply(&data,&c) == AGENDA_OK);
    fail_sync=true; assert(agenda_storage_owner_write(&data) != ESP_OK); fail_sync=false;
    reboot(); assert(agenda_storage_owner_load() == ESP_OK); assert(agenda_storage_copy(&loaded) == ESP_OK);
    assert(!strcmp(loaded.items[0].title,"Primeira geração"));
    fail_rename=true; assert(agenda_storage_owner_write(&data) != ESP_OK); fail_rename=false;
    reboot(); assert(agenda_storage_owner_load() == ESP_OK); assert(agenda_storage_copy(&loaded) == ESP_OK);
    assert(!strcmp(loaded.items[0].title,"Primeira geração"));
    assert(agenda_storage_owner_write(&data) == ESP_OK);
    reboot(); assert(agenda_storage_owner_load() == ESP_OK); assert(agenda_storage_copy(&loaded) == ESP_OK);
    assert(!strcmp(loaded.items[0].title,"Segunda geração"));
    char newest[1024]; path("agenda.0",newest,sizeof(newest));
    FILE *f=fopen(newest,"r+b"); assert(f); assert(fseek(f,30,SEEK_SET) == 0); fputc(0xff,f); fclose(f);
    reboot(); assert(agenda_storage_owner_load() == ESP_OK); assert(agenda_storage_copy(&loaded) == ESP_OK);
    assert(!strcmp(loaded.items[0].title,"Primeira geração"));
    char older[1024]; path("agenda.1",older,sizeof(older)); f=fopen(older,"wb"); assert(f); fputs("bad",f); fclose(f);
    reboot(); assert(agenda_storage_owner_load() == ESP_ERR_INVALID_CRC);
    assert(!agenda_storage_owner_format_allowed());
    assert(agenda_storage_copy(&loaded) == ESP_ERR_INVALID_CRC);
    cleanup(); reboot();
    /* Installed v1 keeps IDs, ETags, pending edits and consumed reminders. */
    legacy_data_t legacy={.instance_id=9U,.next_id=13U,.count=12U};
    for (unsigned i=0U;i<12U;++i) {
        legacy.items[i]=(agenda_item_t){.id=i+1U,.revision=2U,.kind=AGENDA_TASK,
            .reminder_minutes=AGENDA_REMINDER_NONE,.reminded=1U};
        snprintf(legacy.items[i].title,sizeof(legacy.items[i].title),"Legado %u",i);
        snprintf(legacy.remote[i].id,sizeof(legacy.remote[i].id),"remote%u",i);
        strcpy(legacy.remote[i].etag,"etag1"); legacy.remote[i].dirty=1U;
    }
    cache_record_header_t h={.magic=AGENDA_MAGIC,.schema_version=1U,.header_size=sizeof(h),
        .generation=7U,.payload_size=sizeof(legacy)};
    h.payload_crc32=cache_record_crc32((const uint8_t *)&legacy,sizeof(legacy));
    h.header_crc32=cache_record_crc32((const uint8_t *)&h,offsetof(cache_record_header_t,header_crc32));
    path("agenda.1",older,sizeof(older)); f=fopen(older,"wb"); assert(f);
    assert(fwrite(&h,1U,sizeof(h),f) == sizeof(h) && fwrite(&legacy,1U,sizeof(legacy),f) == sizeof(legacy)); fclose(f);
    assert(agenda_storage_owner_load() == ESP_OK && agenda_storage_copy(&loaded) == ESP_OK);
    assert(loaded.instance_id == 9U && loaded.next_id == 13U && loaded.count == 12U);
    assert(!memcmp(legacy.items,loaded.items,sizeof(legacy.items)) && !memcmp(legacy.remote,loaded.remote,sizeof(legacy.remote)));
    c.item=(agenda_item_t){.kind=AGENDA_TASK,.reminder_minutes=AGENDA_REMINDER_NONE}; strcpy(c.item.title,"Novo mês");
    while (loaded.count < AGENDA_MAX_ITEMS) assert(agenda_apply(&loaded,&c) == AGENDA_OK);
    assert(agenda_storage_owner_write(&loaded) == ESP_OK);
    data=loaded; reboot(); assert(agenda_storage_owner_load() == ESP_OK && agenda_storage_copy(&loaded) == ESP_OK);
    assert(!memcmp(&data,&loaded,sizeof(data)) && loaded.count == AGENDA_MAX_ITEMS);
    /* Corrupt v2 falls back to the untouched v1 generation. */
    path("agenda.0",newest,sizeof(newest)); f=fopen(newest,"r+b"); assert(f); fputc(0xff,f); fclose(f);
    reboot(); assert(agenda_storage_owner_load() == ESP_OK && agenda_storage_copy(&loaded) == ESP_OK && loaded.count == 12U);
    cleanup(); reboot(); puts("Agenda storage: v1 migration, 64-item v2 roundtrip, generations, fsync/rename failure, corruption fallback PASS");
    return 0;
}
