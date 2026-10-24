#include "home_widget_preferences.h"
#include "cache_record.h"
#include <stddef.h>
#include <string.h>
#include "nvs.h"
typedef struct { cache_record_header_t header; uint8_t value[4]; } record_t;
static bool valid(const record_t *r)
{
    return cache_record_header_is_valid(&r->header,sizeof(r->value)) && r->header.payload_size==sizeof(r->value) &&
        r->value[0]<=1 && !r->value[1] && !r->value[2] && !r->value[3] &&
        r->header.payload_crc32==cache_record_crc32(r->value,sizeof(r->value));
}
static esp_err_t read(nvs_handle_t handle,record_t *selected)
{
    esp_err_t result=ESP_ERR_NOT_FOUND;
    for(unsigned i=0;i<2;++i) {
        record_t r={0}; size_t size=sizeof(r);
        esp_err_t got=nvs_get_blob(handle,i?"view1":"view0",&r,&size);
        if(got==ESP_ERR_NVS_NOT_FOUND)continue;
        if(got!=ESP_OK || size!=sizeof(r) || !valid(&r)) {if(result!=ESP_OK)result=ESP_ERR_INVALID_CRC;continue;}
        if(result!=ESP_OK || r.header.generation>selected->header.generation) {*selected=r;result=ESP_OK;}
    }
    return result;
}
esp_err_t home_widget_owner_load(uint8_t *widget)
{
    if(!widget)return ESP_ERR_INVALID_ARG;
    nvs_handle_t h; esp_err_t r=nvs_open("np2_home",NVS_READONLY,&h);
    if(r!=ESP_OK)return r==ESP_ERR_NVS_NOT_FOUND?ESP_ERR_NOT_FOUND:r;
    record_t selected={0};r=read(h,&selected);nvs_close(h);
    if(r==ESP_OK)*widget=selected.value[0];
    return r;
}
esp_err_t home_widget_owner_write(uint8_t widget)
{
    if(widget>1)return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;esp_err_t result=nvs_open("np2_home",NVS_READWRITE,&h);
    if(result!=ESP_OK)return result;
    record_t prior={0};result=read(h,&prior);
    if(result!=ESP_OK && result!=ESP_ERR_NOT_FOUND && result!=ESP_ERR_INVALID_CRC){nvs_close(h);return result;}
    if(result==ESP_OK && prior.value[0]==widget){nvs_close(h);return ESP_OK;}
    if(prior.header.generation==UINT32_MAX){nvs_close(h);return ESP_ERR_INVALID_STATE;}
    record_t r={.header={.magic=NP2_CACHE_RECORD_MAGIC,.schema_version=1,.header_size=sizeof(cache_record_header_t),
        .generation=prior.header.generation+1U,.payload_size=4U},.value={widget,0,0,0}};
    r.header.payload_crc32=cache_record_crc32(r.value,sizeof(r.value));
    r.header.header_crc32=cache_record_crc32((const uint8_t *)&r.header,offsetof(cache_record_header_t,header_crc32));
    const char *key=(r.header.generation&1)?"view1":"view0";
    result=nvs_set_blob(h,key,&r,sizeof(r));if(result==ESP_OK)result=nvs_commit(h);
    if(result==ESP_OK){record_t check={0};size_t n=sizeof(check);result=nvs_get_blob(h,key,&check,&n);
        if(result==ESP_OK && (n!=sizeof(check)||memcmp(&r,&check,n)))result=ESP_ERR_INVALID_CRC;}
    nvs_close(h);return result;
}
