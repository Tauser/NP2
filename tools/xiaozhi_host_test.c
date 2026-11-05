/* Host checks for the Xiaozhi protocol codec and the voice resampler. */
#include "xiaozhi_protocol.h"
#include "voice_resampler.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

static unsigned char scratch[16384];

static void protocol(void)
{
    xiaozhi_ota_result_t ota;
    const char *activation = "{\"server_time\":{\"timestamp\":1,\"timezone_offset\":-180},"
        "\"activation\":{\"code\":\"482913\",\"message\":\"xiaozhi.me\\n482913\",\"challenge\":\"abc-123\",\"timeout_ms\":30000},"
        "\"websocket\":{\"url\":\"wss://api.tenclass.net/xiaozhi/v1/\",\"token\":\"test-token\"},"
        "\"firmware\":{\"version\":\"9.9.9\",\"url\":\"https://x/fw.bin\"}}";
    CHECK(xiaozhi_json_scratch_bytes() <= sizeof(scratch));
    CHECK(xiaozhi_parse_ota(scratch, activation, strlen(activation), &ota));
    CHECK(ota.activation && strcmp(ota.code, "482913") == 0 && strcmp(ota.challenge, "abc-123") == 0);
    CHECK(ota.activation_timeout_ms == 30000U);
    CHECK(ota.websocket && strcmp(ota.ws_url, "wss://api.tenclass.net/xiaozhi/v1/") == 0 && strcmp(ota.ws_token, "test-token") == 0);
    const char *plain = "{\"websocket\":{\"url\":\"ws://insecure/\",\"token\":\"t\"}}";
    CHECK(xiaozhi_parse_ota(scratch, plain, strlen(plain), &ota));
    CHECK(!ota.activation && !ota.websocket && ota.ws_token[0] == '\0'); /* only wss:// accepted */
    const char *code = "{\"activation\":{\"code\":\"12a\"}}";
    CHECK(xiaozhi_parse_ota(scratch, code, strlen(code), &ota) && ota.activation && ota.code[0] == '\0');
    CHECK(!xiaozhi_parse_ota(scratch, "{bad", 4, &ota));

    xiaozhi_message_t m;
    const char *hello = "{\"type\":\"hello\",\"transport\":\"websocket\",\"session_id\":\"a1b2-c3\","
        "\"audio_params\":{\"format\":\"opus\",\"sample_rate\":24000,\"channels\":1,\"frame_duration\":60}}";
    CHECK(xiaozhi_parse_message(scratch, hello, strlen(hello), &m) && m.type == XIAOZHI_MSG_HELLO);
    CHECK(m.sample_rate == 24000U && m.frame_duration_ms == 60U && strcmp(m.session_id, "a1b2-c3") == 0);
    const char *mqtt_hello = "{\"type\":\"hello\",\"transport\":\"udp\"}";
    CHECK(!xiaozhi_parse_message(scratch, mqtt_hello, strlen(mqtt_hello), &m));
    const char *bad_rate = "{\"type\":\"hello\",\"transport\":\"websocket\",\"audio_params\":{\"sample_rate\":44100}}";
    CHECK(!xiaozhi_parse_message(scratch, bad_rate, strlen(bad_rate), &m));
    const char *stereo = "{\"type\":\"hello\",\"transport\":\"websocket\",\"audio_params\":{\"channels\":2}}";
    const char *pcm = "{\"type\":\"hello\",\"transport\":\"websocket\",\"audio_params\":{\"format\":\"pcm\"}}";
    const char *long_frame = "{\"type\":\"hello\",\"transport\":\"websocket\",\"audio_params\":{\"frame_duration\":240}}";
    CHECK(!xiaozhi_parse_message(scratch, stereo, strlen(stereo), &m));
    CHECK(!xiaozhi_parse_message(scratch, pcm, strlen(pcm), &m));
    CHECK(!xiaozhi_parse_message(scratch, long_frame, strlen(long_frame), &m));
    const char *stt = "{\"session_id\":\"s\",\"type\":\"stt\",\"text\":\"Qual a previs\\u00e3o?\"}";
    CHECK(xiaozhi_parse_message(scratch, stt, strlen(stt), &m) && m.type == XIAOZHI_MSG_STT);
    CHECK(strcmp(m.text, "Qual a previs\xC3\xA3o?") == 0);
    const char *tts = "{\"session_id\":\"s\",\"type\":\"tts\",\"state\":\"sentence_start\",\"text\":\"Ol\xC3\xA1!\"}";
    CHECK(xiaozhi_parse_message(scratch, tts, strlen(tts), &m) && m.type == XIAOZHI_MSG_TTS_SENTENCE && strcmp(m.text, "Ol\xC3\xA1!") == 0);
    const char *stop = "{\"type\":\"tts\",\"state\":\"stop\"}";
    CHECK(xiaozhi_parse_message(scratch, stop, strlen(stop), &m) && m.type == XIAOZHI_MSG_TTS_STOP);
    const char *llm = "{\"type\":\"llm\",\"emotion\":\"happy\",\"text\":\"x\"}";
    CHECK(xiaozhi_parse_message(scratch, llm, strlen(llm), &m) && m.type == XIAOZHI_MSG_LLM && strcmp(m.emotion, "happy") == 0);
    const char *inject = "{\"type\":\"stt\",\"session_id\":\"a\\\"b\",\"text\":\"t\"}";
    CHECK(xiaozhi_parse_message(scratch, inject, strlen(inject), &m) && m.session_id[0] == '\0');
    /* Long assistant text is truncated on a UTF-8 boundary, never rejected. */
    char long_msg[1200] = "{\"type\":\"tts\",\"state\":\"sentence_start\",\"text\":\"";
    for (int i = 0; i < 200; ++i) strcat(long_msg, "\xC3\xA7");
    strcat(long_msg, "\"}");
    CHECK(xiaozhi_parse_message(scratch, long_msg, strlen(long_msg), &m) && strlen(m.text) < XIAOZHI_TEXT_BYTES);
    CHECK(strlen(m.text) % 2U == 0U);

    char out[256];
    CHECK(xiaozhi_build_hello(out, sizeof(out)) && strstr(out, "\"sample_rate\":16000") && strstr(out, "\"frame_duration\":60"));
    CHECK(strstr(out, "mcp") == NULL);
    CHECK(xiaozhi_build_listen_start(out, sizeof(out), "a1b2-c3", XIAOZHI_LISTEN_AUTO) &&
          strcmp(out, "{\"session_id\":\"a1b2-c3\",\"type\":\"listen\",\"state\":\"start\",\"mode\":\"auto\"}") == 0);
    CHECK(xiaozhi_build_listen_start(out, sizeof(out), "", XIAOZHI_LISTEN_MANUAL));
    CHECK(!xiaozhi_build_listen_stop(out, sizeof(out), "a\"b"));
    CHECK(xiaozhi_build_abort(out, sizeof(out), "s") && strcmp(out, "{\"session_id\":\"s\",\"type\":\"abort\"}") == 0);
    CHECK(!xiaozhi_build_abort(out, 10, "s"));

    const unsigned char mac[6] = {0x30, 0xed, 0xa0, 0x01, 0x8f, 0x42};
    char mac_text[18], uuid[37];
    unsigned char digest[32];
    for (int i = 0; i < 32; ++i) digest[i] = (unsigned char)(i * 37);
    CHECK(xiaozhi_format_mac(mac, mac_text) && strcmp(mac_text, "30:ed:a0:01:8f:42") == 0);
    CHECK(xiaozhi_client_id(digest, uuid) && strlen(uuid) == 36U && uuid[14] == '4' && strchr("89ab", uuid[19]));
    char info[640];
    CHECK(xiaozhi_build_system_info(info, sizeof(info), mac_text, uuid, "39bc170-dirty", "ab12", 33554432U, 100000U));
    CHECK(xiaozhi_parse_ota(scratch, info, strlen(info), &ota)); /* valid JSON */
    CHECK(!xiaozhi_build_system_info(info, sizeof(info), mac_text, uuid, "v\"1", "ab", 1U, 1U));
}

static double tone_gain(uint32_t in_rate, uint32_t out_rate, double hz)
{
    static voice_resampler_t r;
    CHECK(voice_resampler_init(&r, in_rate, out_rate));
    const size_t n = in_rate; /* one second, fed in odd-sized chunks */
    int16_t *in = malloc(n * sizeof(int16_t)), *out = malloc((out_rate + 64U) * sizeof(int16_t));
    for (size_t i = 0; i < n; ++i) in[i] = (int16_t)lrint(10000.0 * sin(2.0 * M_PI * hz * (double)i / in_rate));
    size_t produced = 0U, offset = 0U, chunk = 1U;
    while (offset < n) {
        size_t take = chunk < n - offset ? chunk : n - offset;
        CHECK(voice_resampler_max_output(&r, take) >= (take * out_rate) / in_rate);
        produced += voice_resampler_process(&r, in + offset, take, out + produced, out_rate + 64U - produced);
        offset += take; chunk = chunk * 7U % 383U + 1U;
    }
    CHECK(produced + 16U >= out_rate && produced <= out_rate + 1U);
    double energy = 0.0;
    size_t counted = 0U;
    for (size_t i = out_rate / 10U; i < produced; ++i) { energy += (double)out[i] * out[i]; ++counted; }
    free(in); free(out);
    return sqrt(energy / (double)counted) / (10000.0 / sqrt(2.0));
}

static void resampler(void)
{
    const double pass_down = tone_gain(22050U, 16000U, 1000.0);
    const double stop_down = tone_gain(22050U, 16000U, 10000.0); /* above 8 kHz Nyquist */
    const double pass_up = tone_gain(24000U, 22050U, 3000.0);
    const double pass_up16 = tone_gain(16000U, 22050U, 2000.0);
    printf("resampler 22050->16000 1k=%.4f 10k=%.4f | 24000->22050 3k=%.4f | 16000->22050 2k=%.4f\n",
           pass_down, stop_down, pass_up, pass_up16);
    CHECK(fabs(pass_down - 1.0) < 0.02 && fabs(pass_up - 1.0) < 0.02 && fabs(pass_up16 - 1.0) < 0.02);
    CHECK(stop_down < 0.05);
    voice_resampler_t r;
    CHECK(!voice_resampler_init(&r, 22050U, 96000U));
    CHECK(voice_resampler_init(&r, 22050U, 16000U));
    int16_t full[512], out[512];
    for (int i = 0; i < 512; ++i) full[i] = (i & 1) ? 32767 : -32768;
    (void)voice_resampler_process(&r, full, 512, out, 512); /* saturates, no overflow UB */
    for (int i = 0; i < 512; ++i) full[i] = 32767;
    size_t n = voice_resampler_process(&r, full, 512, out, 512);
    CHECK(n > 0U && out[n - 1U] >= 32700);
}

int main(void)
{
    protocol();
    resampler();
    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("PASS xiaozhi protocol/resampler\n");
    return 0;
}
