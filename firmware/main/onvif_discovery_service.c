#include "onvif_discovery_service.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include "camera_inventory.h"
#include "connectivity_diagnostic.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "flash_coordinator.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"

#define ONVIF_TASK_STACK_BYTES (10U * 1024U)
#define ONVIF_STACK_WARN_FREE_BYTES 1536U
#define ONVIF_TASK_PRIORITY 2U
#define ONVIF_SCAN_TIMEOUT_MS 2500U
#define ONVIF_REFRESH_INTERVAL_US UINT64_C(60000000)
#define ONVIF_POLL_MS 100U
#define ONVIF_MAX_RESPONSE_BYTES 2047U

static const char *const TAG = "onvif_discovery";
static const struct sockaddr_in s_discovery_target = {
    .sin_family = AF_INET,
    .sin_port = PP_HTONS(3702),
    .sin_addr = {.s_addr = PP_HTONL(LWIP_MAKEU32(239, 255, 255, 250))},
};
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static onvif_discovery_status_t s_status = {
    .last_result = ESP_ERR_INVALID_STATE,
};
static bool s_started;
static bool s_scan_requested;
static bool s_periodic_refresh_enabled;
static uint64_t s_last_scan_us;
static char s_manual_addresses[ONVIF_DISCOVERY_MAX_CAMERAS][16];
static bool s_inventory_restored;
static bool s_sweep_requested;
static onvif_sweep_status_t s_sweep;

/* lwIP is built with 10 sockets and the panel needs the rest, so probe a few at a time. */
#define SWEEP_BATCH 6U
/* A Wi-Fi camera that sleeps between beacons can need one ARP retry (about a second) before the
 * first connect completes, so a short wait finds nothing. */
#define SWEEP_CONNECT_TIMEOUT_US 1300000
#define SWEEP_RTSP_PORT 554U
#define SWEEP_ONVIF_PORT 2020U

static bool xml_name_break(char character)
{
    return character == '\0' || character == '>' || character == '/' ||
        character == ' ' || character == '\t' || character == '\r' ||
        character == '\n';
}

static bool extract_local_tag(const char *xml, const char *local_name,
                              const char **out_value, size_t *out_length)
{
    if (xml == NULL || local_name == NULL || out_value == NULL ||
        out_length == NULL) {
        return false;
    }
    const size_t local_name_length = strlen(local_name);
    const char *cursor = xml;
    const char *open = NULL;
    while ((open = strchr(cursor, '<')) != NULL) {
        const char *const name = open + 1;
        if (*name == '/' || *name == '?' || *name == '!') {
            cursor = name;
            continue;
        }
        const char *name_end = name;
        while (!xml_name_break(*name_end)) ++name_end;
        const char *local_start = name;
        for (const char *part = name; part < name_end; ++part) {
            if (*part == ':') local_start = part + 1;
        }
        const size_t qualified_length = (size_t)(name_end - name);
        const size_t local_length = (size_t)(name_end - local_start);
        if (local_length == local_name_length &&
            memcmp(local_start, local_name, local_name_length) == 0) {
            char close_tag[68] = {0};
            const int close_length = snprintf(close_tag, sizeof(close_tag),
                "</%.*s>", (int)qualified_length, name);
            const char *const value = strchr(name_end, '>');
            if (close_length > 0 && (size_t)close_length < sizeof(close_tag) &&
                value != NULL) {
                const char *const content = value + 1;
                const char *const close = strstr(content, close_tag);
                if (close != NULL && close != content) {
                    *out_value = content;
                    *out_length = (size_t)(close - content);
                    return true;
                }
            }
        }
        cursor = name_end;
    }
    return false;
}

static bool response_is_probe_match(const char *response)
{
    const char *value = NULL;
    size_t length = 0U;
    return extract_local_tag(response, "ProbeMatch", &value, &length);
}

static bool response_is_video_transmitter(const char *response)
{
    return response != NULL && strstr(response, "NetworkVideoTransmitter") != NULL;
}

static bool parse_ipv4_xaddr(const char *xaddrs, size_t length,
                             char *out_address, size_t out_size);

static bool parse_probe_match(const char *response, onvif_camera_t *out_camera)
{
    if (!response_is_probe_match(response) ||
        !response_is_video_transmitter(response) || out_camera == NULL) {
        return false;
    }
    const char *xaddrs = NULL;
    size_t xaddrs_length = 0U;
    if (!extract_local_tag(response, "XAddrs", &xaddrs, &xaddrs_length) ||
        !parse_ipv4_xaddr(xaddrs, xaddrs_length, out_camera->address,
                          sizeof(out_camera->address))) {
        return false;
    }

    const char *scopes = NULL;
    size_t scopes_length = 0U;
    if (extract_local_tag(response, "Scopes", &scopes, &scopes_length)) {
        bool is_tapo_c200 = false;
        for (size_t i = 0U; i + 9U <= scopes_length; ++i) {
            if (memcmp(scopes + i, "Tapo_C200", 9U) == 0) {
                is_tapo_c200 = true;
                break;
            }
        }
        if (is_tapo_c200) {
            (void)snprintf(out_camera->model, sizeof(out_camera->model), "Tapo C200");
        }
    }
    if (out_camera->model[0] == '\0') {
        (void)snprintf(out_camera->model, sizeof(out_camera->model), "Câmera ONVIF");
    }
    return true;
}

static bool parse_ipv4_xaddr(const char *xaddrs, size_t length,
                             char *out_address, size_t out_size)
{
    if (xaddrs == NULL || out_address == NULL || out_size == 0U) return false;
    const char *cursor = xaddrs;
    const char *const end = xaddrs + length;
    while (cursor < end) {
        const char *host = NULL;
        for (const char *p = cursor; p + 7U < end; ++p) {
            if (memcmp(p, "http://", 7U) == 0) {
                host = p + 7U;
                break;
            }
        }
        if (host == NULL) return false;
        const char *host_end = host;
        while (host_end < end && *host_end != ':' && *host_end != '/' &&
               *host_end != ' ' && *host_end != '\t' && *host_end != '<') {
            ++host_end;
        }
        const size_t host_length = (size_t)(host_end - host);
        if (host_length > 0U && host_length < out_size) {
            char candidate[16] = {0};
            if (host_length < sizeof(candidate)) {
                memcpy(candidate, host, host_length);
                struct in_addr parsed = {0};
                if (inet_aton(candidate, &parsed) != 0) {
                    memcpy(out_address, candidate, host_length + 1U);
                    return true;
                }
            }
        }
        cursor = host_end;
    }
    return false;
}

static bool onvif_endpoint_reachable(const char *address)
{
    struct in_addr ipv4 = {0};
    if (address == NULL || inet_pton(AF_INET, address, &ipv4) != 1) return false;
    const int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) return false;

    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        (void)close(fd);
        return false;
    }
    const struct sockaddr_in endpoint = {
        .sin_family = AF_INET,
        .sin_port = PP_HTONS(2020),
        .sin_addr = ipv4,
    };
    bool reachable = connect(fd, (const struct sockaddr *)&endpoint,
                             sizeof(endpoint)) == 0;
    if (!reachable && (errno == EINPROGRESS || errno == EWOULDBLOCK)) {
        fd_set writable;
        FD_ZERO(&writable);
        FD_SET(fd, &writable);
        struct timeval timeout = {.tv_sec = 0, .tv_usec = 750000};
        const int selected = select(fd + 1, NULL, &writable, NULL, &timeout);
        if (selected > 0 && FD_ISSET(fd, &writable)) {
            int socket_error = 0;
            socklen_t error_size = sizeof(socket_error);
            if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &socket_error,
                           &error_size) == 0 && socket_error == 0) {
                reachable = true;
            }
        }
    }
    (void)close(fd);
    return reachable;
}


static esp_err_t make_probe(uint32_t nonce, char *out, size_t out_size)
{
    const int written = snprintf(out, out_size,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<e:Envelope xmlns:e=\"http://www.w3.org/2003/05/soap-envelope\" "
        "xmlns:w=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" "
        "xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\" "
        "xmlns:dn=\"http://www.onvif.org/ver10/network/wsdl\">"
        "<e:Header><w:MessageID>uuid:%08lx-%08lx-0000-0000-000000000001</w:MessageID>"
        "<w:To e:mustUnderstand=\"true\">urn:schemas-xmlsoap-org:ws:2005:04:discovery</w:To>"
        "<w:Action e:mustUnderstand=\"true\">http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe</w:Action>"
        "</e:Header><e:Body><d:Probe><d:Types>dn:NetworkVideoTransmitter</d:Types>"
        "</d:Probe></e:Body></e:Envelope>",
        (unsigned long)nonce, (unsigned long)esp_random());
    if (written <= 0 || (size_t)written >= out_size) return ESP_ERR_INVALID_SIZE;
    return ESP_OK;
}

static void scan_cameras(void)
{
    char manual_addresses[ONVIF_DISCOVERY_MAX_CAMERAS][16] = {{0}};
    taskENTER_CRITICAL(&s_lock);
    memcpy(manual_addresses, s_manual_addresses, sizeof(manual_addresses));
    taskEXIT_CRITICAL(&s_lock);

    connectivity_diagnostic_status_t network = {0};
    connectivity_diagnostic_get_status(&network);
    if (!network.online) {
        taskENTER_CRITICAL(&s_lock);
        s_status.last_result = ESP_ERR_INVALID_STATE;
        for (uint8_t i = 0U; i < s_status.camera_count; ++i) {
            s_status.cameras[i].online = false;
        }
        s_status.scan_busy = false;
        s_status.scan_generation++;
        taskEXIT_CRITICAL(&s_lock);
        return;
    }

    const int socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_fd < 0) {
        taskENTER_CRITICAL(&s_lock);
        s_status.last_result = ESP_FAIL;
        for (uint8_t i = 0U; i < s_status.camera_count; ++i) {
            s_status.cameras[i].online = false;
        }
        s_status.scan_busy = false;
        s_status.scan_generation++;
        taskEXIT_CRITICAL(&s_lock);
        return;
    }
    const int ttl = 1;
    const struct timeval timeout = {
        .tv_sec = 0,
        .tv_usec = (suseconds_t)(ONVIF_SCAN_TIMEOUT_MS * 1000U),
    };
    (void)setsockopt(socket_fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
    (void)setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    char probe[768] = {0};
    const esp_err_t probe_result = make_probe(
        (uint32_t)((uint64_t)esp_timer_get_time() & UINT32_MAX), probe, sizeof(probe));
    const ssize_t sent = probe_result == ESP_OK
        ? sendto(socket_fd, probe, strlen(probe), 0,
                 (const struct sockaddr *)&s_discovery_target,
                 sizeof(s_discovery_target))
        : -1;
    memset(probe, 0, sizeof(probe));

    onvif_camera_t found[ONVIF_DISCOVERY_MAX_CAMERAS] = {0};
    uint8_t count = 0U;
    uint16_t datagrams_received = 0U;
    uint16_t probe_matches = 0U;
    uint16_t rejected_responses = 0U;
    uint8_t direct_endpoints_up = 0U;
    esp_err_t result = sent > 0 ? ESP_OK : (probe_result != ESP_OK ? probe_result : ESP_FAIL);
    while (sent > 0 && count < ONVIF_DISCOVERY_MAX_CAMERAS) {
        char response[ONVIF_MAX_RESPONSE_BYTES + 1U] = {0};
        struct sockaddr_in source = {0};
        socklen_t source_length = sizeof(source);
        const ssize_t received = recvfrom(socket_fd, response,
            ONVIF_MAX_RESPONSE_BYTES, 0, (struct sockaddr *)&source, &source_length);
        if (received <= 0) break;
        datagrams_received++;
        response[received] = '\0';
        if (response_is_probe_match(response)) probe_matches++;
        onvif_camera_t camera = {0};
        const bool parsed = parse_probe_match(response, &camera);
        memset(response, 0, sizeof(response));
        if (!parsed || camera.address[0] == '\0') {
            rejected_responses++;
            continue;
        }
        bool duplicate = false;
        for (uint8_t i = 0U; i < count; ++i) {
            duplicate |= strcmp(found[i].address, camera.address) == 0;
        }
        if (!duplicate) {
            camera.online = true;
            found[count++] = camera;
        }
    }
    for (uint8_t target = 0U; target < ONVIF_DISCOVERY_MAX_CAMERAS; ++target) {
        const char *const address = manual_addresses[target];
        if (address[0] == '\0') continue;
        if (onvif_endpoint_reachable(address)) {
            direct_endpoints_up++;
            bool duplicate = false;
            for (uint8_t i = 0U; i < count; ++i) {
                duplicate |= strcmp(found[i].address, address) == 0;
            }
            if (!duplicate && count < ONVIF_DISCOVERY_MAX_CAMERAS) {
                onvif_camera_t camera = {0};
                (void)snprintf(camera.address, sizeof(camera.address), "%s",
                               address);
                (void)snprintf(camera.model, sizeof(camera.model), "Tapo C200");
                camera.online = true;
                found[count++] = camera;
            }
        }
    }
    (void)close(socket_fd);

    taskENTER_CRITICAL(&s_lock);
    for (uint8_t old = 0U; old < s_status.camera_count; ++old) {
        s_status.cameras[old].online = false;
        for (uint8_t current = 0U; current < count; ++current) {
            if (strcmp(s_status.cameras[old].address, found[current].address) == 0) {
                s_status.cameras[old] = found[current];
                break;
            }
        }
    }
    for (uint8_t current = 0U; current < count; ++current) {
        bool known = false;
        for (uint8_t old = 0U; old < s_status.camera_count; ++old) {
            known |= strcmp(s_status.cameras[old].address,
                            found[current].address) == 0;
        }
        if (!known && s_status.camera_count < ONVIF_DISCOVERY_MAX_CAMERAS) {
            s_status.cameras[s_status.camera_count++] = found[current];
        }
    }
    s_status.last_result = result;
    s_status.scan_busy = false;
    s_status.scan_generation++;
    taskEXIT_CRITICAL(&s_lock);
    ESP_LOGI(TAG,
             "ONVIF discovery complete result=%s cameras=%u datagrams=%u "
             "probe_matches=%u rejected=%u direct_endpoints_up=%u",
             esp_err_to_name(result), (unsigned int)count,
             (unsigned int)datagrams_received, (unsigned int)probe_matches,
             (unsigned int)rejected_responses,
             (unsigned int)direct_endpoints_up);
    const UBaseType_t stack_free_bytes = uxTaskGetStackHighWaterMark(NULL);
    if (stack_free_bytes < ONVIF_STACK_WARN_FREE_BYTES) {
        ESP_LOGW(TAG, "low task stack margin: free=%u bytes",
                 (unsigned int)stack_free_bytes);
    } else {
        ESP_LOGI(TAG, "task stack margin free=%u bytes",
                 (unsigned int)stack_free_bytes);
    }
    memset(found, 0, sizeof(found));
}

static bool private_address(uint32_t host_order)
{
    return (host_order & UINT32_C(0xff000000)) == UINT32_C(0x0a000000) ||
           (host_order & UINT32_C(0xfff00000)) == UINT32_C(0xac100000) ||
           (host_order & UINT32_C(0xffff0000)) == UINT32_C(0xc0a80000);
}

typedef enum {
    PROBE_SILENT = 0, /* No answer in time: absent, asleep or filtering. */
    PROBE_OPEN,
    PROBE_REFUSED,    /* Alive, but the port is closed. */
} probe_outcome_t;

/* Non-blocking connects to one port on a few hosts, waiting up to the timeout for all of them. */
static void probe_port_batch(const uint32_t *hosts_host_order, uint8_t count, uint16_t port,
                             probe_outcome_t *outcome)
{
    int fds[SWEEP_BATCH];
    uint8_t pending = 0U;
    for (uint8_t i = 0U; i < count; ++i) {
        outcome[i] = PROBE_SILENT;
        fds[i] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (fds[i] < 0) continue;
        const int flags = fcntl(fds[i], F_GETFL, 0);
        if (flags < 0 || fcntl(fds[i], F_SETFL, flags | O_NONBLOCK) < 0) {
            (void)close(fds[i]);
            fds[i] = -1;
            continue;
        }
        const struct sockaddr_in endpoint = {
            .sin_family = AF_INET,
            .sin_port = htons(port),
            .sin_addr.s_addr = htonl(hosts_host_order[i]),
        };
        if (connect(fds[i], (const struct sockaddr *)&endpoint, sizeof(endpoint)) == 0) {
            outcome[i] = PROBE_OPEN;
            (void)close(fds[i]);
            fds[i] = -1;
        } else if (errno == EINPROGRESS || errno == EWOULDBLOCK) {
            ++pending;
        } else {
            if (errno == ECONNREFUSED) outcome[i] = PROBE_REFUSED;
            (void)close(fds[i]);
            fds[i] = -1;
        }
    }
    const int64_t deadline = esp_timer_get_time() + SWEEP_CONNECT_TIMEOUT_US;
    while (pending > 0U) {
        const int64_t left = deadline - esp_timer_get_time();
        if (left <= 0) break;
        fd_set writable, failed;
        FD_ZERO(&writable);
        FD_ZERO(&failed);
        int max_fd = -1;
        for (uint8_t i = 0U; i < count; ++i) {
            if (fds[i] < 0) continue;
            FD_SET(fds[i], &writable);
            FD_SET(fds[i], &failed);
            if (fds[i] > max_fd) max_fd = fds[i];
        }
        struct timeval timeout = {.tv_sec = (time_t)(left / 1000000), .tv_usec = (suseconds_t)(left % 1000000)};
        if (select(max_fd + 1, NULL, &writable, &failed, &timeout) <= 0) break;
        for (uint8_t i = 0U; i < count; ++i) {
            if (fds[i] < 0 || (!FD_ISSET(fds[i], &writable) && !FD_ISSET(fds[i], &failed))) continue;
            int socket_error = 0;
            socklen_t error_size = sizeof(socket_error);
            if (getsockopt(fds[i], SOL_SOCKET, SO_ERROR, &socket_error, &error_size) == 0) {
                if (socket_error == 0) outcome[i] = PROBE_OPEN;
                else if (socket_error == ECONNREFUSED) outcome[i] = PROBE_REFUSED;
            }
            (void)close(fds[i]);
            fds[i] = -1;
            --pending;
        }
    }
    for (uint8_t i = 0U; i < count; ++i) {
        if (fds[i] >= 0) (void)close(fds[i]);
    }
}

/* Port 554 alone also matches NAS boxes and routers, so ask for an RTSP answer. */
static bool rtsp_server_answers(uint32_t host_order)
{
    const int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) return false;
    const struct timeval timeout = {.tv_sec = 2, .tv_usec = 0};
    (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    const struct sockaddr_in endpoint = {
        .sin_family = AF_INET,
        .sin_port = htons(SWEEP_RTSP_PORT),
        .sin_addr.s_addr = htonl(host_order),
    };
    bool answers = false;
    if (connect(fd, (const struct sockaddr *)&endpoint, sizeof(endpoint)) == 0) {
        static const char request[] = "OPTIONS * RTSP/1.0\r\nCSeq: 1\r\nUser-Agent: NovaPanel/1.0\r\n\r\n";
        char reply[16] = {0};
        answers = send(fd, request, sizeof(request) - 1U, 0) > 0 &&
                  recv(fd, reply, sizeof(reply) - 1U, 0) >= 8 && strncmp(reply, "RTSP/1.", 7) == 0;
    }
    (void)close(fd);
    return answers;
}

static void finish_sweep(esp_err_t result)
{
    taskENTER_CRITICAL(&s_lock);
    s_sweep.busy = false;
    s_sweep.progress_percent = 100U;
    s_sweep.last_result = result;
    ++s_sweep.generation;
    taskEXIT_CRITICAL(&s_lock);
}

static void publish_candidate(uint32_t host_order)
{
    probe_outcome_t onvif = PROBE_SILENT;
    const uint32_t host[1] = {host_order};
    probe_port_batch(host, 1U, SWEEP_ONVIF_PORT, &onvif);
    onvif_candidate_t candidate = {0};
    struct in_addr address = {.s_addr = htonl(host_order)};
    (void)inet_ntop(AF_INET, &address, candidate.address, sizeof(candidate.address));
    (void)snprintf(candidate.label, sizeof(candidate.label), "%s",
                   onvif == PROBE_OPEN ? "Câmera ONVIF" : "Câmera RTSP");
    taskENTER_CRITICAL(&s_lock);
    if (s_sweep.count < ONVIF_DISCOVERY_MAX_CANDIDATES) s_sweep.candidates[s_sweep.count++] = candidate;
    taskEXIT_CRITICAL(&s_lock);
}

static void run_sweep(void)
{
    taskENTER_CRITICAL(&s_lock);
    s_sweep.count = 0U;
    s_sweep.progress_percent = 0U;
    taskEXIT_CRITICAL(&s_lock);

    connectivity_diagnostic_status_t network = {0};
    connectivity_diagnostic_get_status(&network);
    esp_netif_t *const station = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip_info = {0};
    if (!network.online || station == NULL || esp_netif_get_ip_info(station, &ip_info) != ESP_OK ||
        ip_info.ip.addr == 0U) {
        finish_sweep(ESP_ERR_INVALID_STATE);
        return;
    }
    const uint32_t own = ntohl(ip_info.ip.addr);
    if (!private_address(own)) {
        finish_sweep(ESP_ERR_INVALID_STATE);
        return;
    }
    /* Only the panel's own /24: 254 hosts is already close to a minute on a quiet network. */
    const uint32_t network_base = own & UINT32_C(0xffffff00);
    const int own_offset = (int)(own - network_base);

    /* Nearest addresses first: DHCP hands out neighbouring leases, so cameras tend to be found
     * early and the list fills while the user is still typing. */
    uint8_t order[254];
    uint16_t total = 0U;
    for (int distance = 1; distance <= 254; ++distance) {
        const int up = own_offset + distance;
        const int down = own_offset - distance;
        if (up >= 1 && up <= 254) order[total++] = (uint8_t)up;
        if (down >= 1 && down <= 254) order[total++] = (uint8_t)down;
    }

    const int64_t started_us = esp_timer_get_time();
    uint16_t done = 0U;
    uint16_t open = 0U, refused = 0U, silent = 0U;
    while (done < total) {
        uint32_t hosts[SWEEP_BATCH];
        probe_outcome_t outcome[SWEEP_BATCH];
        uint8_t batch = 0U;
        while (batch < SWEEP_BATCH && done + batch < total) {
            hosts[batch] = network_base + order[done + batch];
            ++batch;
        }
        probe_port_batch(hosts, batch, SWEEP_RTSP_PORT, outcome);
        for (uint8_t i = 0U; i < batch; ++i) {
            if (outcome[i] == PROBE_OPEN) {
                ++open;
                if (rtsp_server_answers(hosts[i])) publish_candidate(hosts[i]);
            } else if (outcome[i] == PROBE_REFUSED) {
                ++refused;
            } else {
                ++silent;
            }
        }
        done = (uint16_t)(done + batch);
        taskENTER_CRITICAL(&s_lock);
        s_sweep.progress_percent = (uint8_t)((uint32_t)done * 100U / total);
        const bool full = s_sweep.count >= ONVIF_DISCOVERY_MAX_CANDIDATES;
        taskEXIT_CRITICAL(&s_lock);
        if (full) break;
    }
    onvif_sweep_status_t result = {0};
    onvif_discovery_service_get_sweep(&result);
    ESP_LOGI(TAG, "Subnet sweep complete hosts=%u open=%u refused=%u silent=%u cameras=%u ms=%lu",
             (unsigned int)done, (unsigned int)open, (unsigned int)refused, (unsigned int)silent,
             (unsigned int)result.count, (unsigned long)((esp_timer_get_time() - started_us) / 1000));
    finish_sweep(ESP_OK);
}

_Static_assert(ONVIF_DISCOVERY_MAX_CAMERAS == CAMERA_INVENTORY_MAX,
               "discovery and saved inventory must hold the same number of cameras");

/* Brings back the cameras saved in flash. They start offline: only a scan marks one online.
 * Saved addresses also join the direct-probe list, so a camera that does not answer
 * WS-Discovery after a restart is still found. */
static void restore_inventory(void)
{
    if (s_inventory_restored || !camera_inventory_load_attempted()) return;
    camera_inventory_t saved = {0};
    camera_inventory_get(&saved);
    taskENTER_CRITICAL(&s_lock);
    for (uint8_t i = 0U; i < saved.count; ++i) {
        bool known = false;
        for (uint8_t old = 0U; old < s_status.camera_count; ++old) {
            known |= strcmp(s_status.cameras[old].address, saved.cameras[i].address) == 0;
        }
        if (!known && s_status.camera_count < ONVIF_DISCOVERY_MAX_CAMERAS) {
            onvif_camera_t *const camera = &s_status.cameras[s_status.camera_count++];
            memcpy(camera->address, saved.cameras[i].address, sizeof(camera->address));
            memcpy(camera->model, saved.cameras[i].model, sizeof(camera->model));
            camera->online = false;
        }
        for (uint8_t slot = 0U; slot < ONVIF_DISCOVERY_MAX_CAMERAS; ++slot) {
            if (strcmp(s_manual_addresses[slot], saved.cameras[i].address) == 0) break;
            if (s_manual_addresses[slot][0] == '\0') {
                memcpy(s_manual_addresses[slot], saved.cameras[i].address,
                       sizeof(s_manual_addresses[slot]));
                break;
            }
        }
    }
    s_inventory_restored = true;
    if (saved.count > 0U) s_scan_requested = true; /* Quiet: no spinner for a background refresh. */
    taskEXIT_CRITICAL(&s_lock);
    ESP_LOGI(TAG, "Saved cameras restored count=%u", (unsigned int)saved.count);
}

/* Saves only when the list differs from flash. Runs here, never from a touch callback. */
static void persist_inventory_if_changed(void)
{
    if (!s_inventory_restored) return;
    camera_inventory_t current = {0};
    taskENTER_CRITICAL(&s_lock);
    current.count = s_status.camera_count;
    for (uint8_t i = 0U; i < s_status.camera_count; ++i) {
        memcpy(current.cameras[i].address, s_status.cameras[i].address,
               sizeof(current.cameras[i].address));
        memcpy(current.cameras[i].model, s_status.cameras[i].model,
               sizeof(current.cameras[i].model));
    }
    taskEXIT_CRITICAL(&s_lock);
    camera_inventory_t saved = {0};
    camera_inventory_get(&saved);
    if (memcmp(&current, &saved, sizeof(current)) == 0) return;
    /* A busy coordinator is normal; the next poll tries again. */
    (void)flash_coordinator_request_camera_inventory_write(&current);
}

static void onvif_worker(void *arg)
{
    (void)arg;
    for (;;) {
        restore_inventory();
        persist_inventory_if_changed();
        bool sweep = false;
        taskENTER_CRITICAL(&s_lock);
        sweep = s_sweep_requested;
        s_sweep_requested = false;
        taskEXIT_CRITICAL(&s_lock);
        if (sweep) run_sweep();
        bool requested = false;
        bool periodic_refresh = false;
        taskENTER_CRITICAL(&s_lock);
        requested = s_scan_requested;
        s_scan_requested = false;
        periodic_refresh = s_periodic_refresh_enabled &&
            s_status.scan_generation > 0U &&
            (uint64_t)esp_timer_get_time() - s_last_scan_us >=
                ONVIF_REFRESH_INTERVAL_US;
        taskEXIT_CRITICAL(&s_lock);
        if (requested || periodic_refresh) {
            scan_cameras();
            s_last_scan_us = (uint64_t)esp_timer_get_time();
            if (requested) {
                taskENTER_CRITICAL(&s_lock);
                s_periodic_refresh_enabled = true;
                taskEXIT_CRITICAL(&s_lock);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(ONVIF_POLL_MS));
    }
}

esp_err_t onvif_discovery_service_start(void)
{
    if (s_started) return ESP_OK;
    if (xTaskCreate(onvif_worker, "onvif_discovery", ONVIF_TASK_STACK_BYTES,
                    NULL, ONVIF_TASK_PRIORITY, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    s_started = true;
    taskENTER_CRITICAL(&s_lock);
    s_status.ready = true;
    taskEXIT_CRITICAL(&s_lock);
    return ESP_OK;
}

esp_err_t onvif_discovery_service_request_scan(void)
{
    if (!s_started) return ESP_ERR_INVALID_STATE;
    taskENTER_CRITICAL(&s_lock);
    if (s_status.scan_busy || s_scan_requested) {
        taskEXIT_CRITICAL(&s_lock);
        return ESP_ERR_TIMEOUT;
    }
    s_status.scan_busy = true;
    s_status.last_result = ESP_ERR_TIMEOUT;
    s_scan_requested = true;
    taskEXIT_CRITICAL(&s_lock);
    return ESP_OK;
}

esp_err_t onvif_discovery_service_request_address(const char *address)
{
    if (!s_started || address == NULL) return ESP_ERR_INVALID_ARG;
    const size_t length = strnlen(address, sizeof(s_manual_addresses[0]));
    if (length == 0U || length >= sizeof(s_manual_addresses[0])) {
        return ESP_ERR_INVALID_ARG;
    }
    struct in_addr ipv4 = {0};
    if (inet_pton(AF_INET, address, &ipv4) != 1) return ESP_ERR_INVALID_ARG;
    const uint32_t host_order = ntohl(ipv4.s_addr);
    const bool private_ipv4 =
        (host_order & UINT32_C(0xff000000)) == UINT32_C(0x0a000000) ||
        (host_order & UINT32_C(0xfff00000)) == UINT32_C(0xac100000) ||
        (host_order & UINT32_C(0xffff0000)) == UINT32_C(0xc0a80000);
    if (!private_ipv4) return ESP_ERR_INVALID_ARG;

    taskENTER_CRITICAL(&s_lock);
    if (s_status.scan_busy || s_scan_requested) {
        taskEXIT_CRITICAL(&s_lock);
        return ESP_ERR_TIMEOUT;
    }
    uint8_t free_slot = ONVIF_DISCOVERY_MAX_CAMERAS;
    for (uint8_t i = 0U; i < ONVIF_DISCOVERY_MAX_CAMERAS; ++i) {
        if (strcmp(s_manual_addresses[i], address) == 0) {
            free_slot = i;
            break;
        }
        if (free_slot == ONVIF_DISCOVERY_MAX_CAMERAS &&
            s_manual_addresses[i][0] == '\0') free_slot = i;
    }
    if (free_slot == ONVIF_DISCOVERY_MAX_CAMERAS) {
        taskEXIT_CRITICAL(&s_lock);
        return ESP_ERR_NO_MEM;
    }
    memcpy(s_manual_addresses[free_slot], address, length + 1U);
    s_status.scan_busy = true;
    s_status.last_result = ESP_ERR_TIMEOUT;
    s_scan_requested = true;
    taskEXIT_CRITICAL(&s_lock);
    return ESP_OK;
}

esp_err_t onvif_discovery_service_request_sweep(void)
{
    if (!s_started) return ESP_ERR_INVALID_STATE;
    taskENTER_CRITICAL(&s_lock);
    if (s_sweep.busy) {
        taskEXIT_CRITICAL(&s_lock);
        return ESP_ERR_TIMEOUT;
    }
    s_sweep.busy = true;
    s_sweep.progress_percent = 0U;
    s_sweep.count = 0U;
    s_sweep_requested = true;
    taskEXIT_CRITICAL(&s_lock);
    return ESP_OK;
}

void onvif_discovery_service_get_sweep(onvif_sweep_status_t *out_status)
{
    if (out_status == NULL) return;
    taskENTER_CRITICAL(&s_lock);
    *out_status = s_sweep;
    taskEXIT_CRITICAL(&s_lock);
}

esp_err_t onvif_discovery_service_add_camera(const char *address, const char *model)
{
    if (!s_started || address == NULL) return ESP_ERR_INVALID_ARG;
    struct in_addr ipv4 = {0};
    if (strnlen(address, 16U) >= 16U || inet_pton(AF_INET, address, &ipv4) != 1 ||
        !private_address(ntohl(ipv4.s_addr))) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t result = ESP_OK;
    taskENTER_CRITICAL(&s_lock);
    uint8_t slot = s_status.camera_count;
    for (uint8_t i = 0U; i < s_status.camera_count; ++i) {
        if (strcmp(s_status.cameras[i].address, address) == 0) {
            slot = i;
            break;
        }
    }
    if (slot >= ONVIF_DISCOVERY_MAX_CAMERAS) {
        result = ESP_ERR_NO_MEM;
    } else {
        onvif_camera_t *const camera = &s_status.cameras[slot];
        if (slot == s_status.camera_count) ++s_status.camera_count;
        (void)snprintf(camera->address, sizeof(camera->address), "%s", address);
        if (camera->model[0] == '\0' || model != NULL) {
            (void)snprintf(camera->model, sizeof(camera->model), "%s",
                           model != NULL && model[0] != '\0' ? model : "Câmera");
        }
        camera->online = true;
        /* Direct probing keeps its online dot honest even where multicast is ignored. */
        for (uint8_t i = 0U; i < ONVIF_DISCOVERY_MAX_CAMERAS; ++i) {
            if (strcmp(s_manual_addresses[i], address) == 0) break;
            if (s_manual_addresses[i][0] == '\0') {
                (void)snprintf(s_manual_addresses[i], sizeof(s_manual_addresses[i]), "%s", address);
                break;
            }
        }
    }
    taskEXIT_CRITICAL(&s_lock);
    return result;
}

void onvif_discovery_service_get_status(onvif_discovery_status_t *out_status)
{
    if (out_status == NULL) return;
    taskENTER_CRITICAL(&s_lock);
    *out_status = s_status;
    taskEXIT_CRITICAL(&s_lock);
}
