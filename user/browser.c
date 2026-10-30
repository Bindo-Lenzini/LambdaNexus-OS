#include "user.h"
#include "libc.h"
#include "dns.h"

#define RESPONSE_MAX 8192
#define PAGE_MAX 6144

static char response[RESPONSE_MAX];
static char page[PAGE_MAX];

static int copy_part(char *dst, int cap, const char *src, int len) {
    if (cap <= 0 || len < 0) return 0;
    if (len >= cap) len = cap - 1;
    for (int i = 0; i < len; i++) dst[i] = src[i];
    dst[len] = 0;
    return len;
}

static int starts_with(const char *s, const char *prefix) {
    while (*prefix) {
        if (*s++ != *prefix++) return 0;
    }
    return 1;
}

static int contains(const char *s, const char *needle) {
    for (int i = 0; s[i]; i++) {
        int j = 0;
        while (needle[j] && s[i + j] == needle[j]) j++;
        if (!needle[j]) return i;
    }
    return -1;
}

static int append_char(char *dst, int cap, int len, char c) {
    if (len < cap - 1) dst[len++] = c;
    return len;
}

static int tag_is_block(const char *tag, int len) {
    static const char *blocks[] = {"p", "div", "br", "li", "h1", "h2",
                                   "h3", "tr", "section", "article"};
    int start = (len > 0 && tag[0] == '/') ? 1 : 0;
    int name_len = 0;
    while (start + name_len < len && tag[start + name_len] != ' ' &&
           tag[start + name_len] != '/' && tag[start + name_len] != '\t') {
        name_len++;
    }
    for (unsigned int i = 0; i < sizeof(blocks) / sizeof(blocks[0]); i++) {
        int j = 0;
        while (blocks[i][j] && j < name_len &&
               tag[start + j] == blocks[i][j]) j++;
        if (j == name_len && blocks[i][j] == 0) return 1;
    }
    return 0;
}

static int is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static int html_to_text(const char *src, int src_len, char *out, int out_cap) {
    int out_len = 0;
    int pending_space = 0;
    int in_script = 0;
    int in_style = 0;

    for (int i = 0; i < src_len && out_len < out_cap - 2; i++) {
        char c = src[i];
        if (c == '<') {
            int tag_start = i + 1;
            int tag_end = tag_start;
            while (tag_end < src_len && src[tag_end] != '>') tag_end++;
            if (tag_end >= src_len) break;
            int tag_len = tag_end - tag_start;
            const char *tag = src + tag_start;
            if (tag_len >= 6 && starts_with(tag, "script")) in_script = 1;
            if (tag_len >= 7 && starts_with(tag, "/script")) in_script = 0;
            if (tag_len >= 5 && starts_with(tag, "style")) in_style = 1;
            if (tag_len >= 6 && starts_with(tag, "/style")) in_style = 0;
            if (!in_script && !in_style && tag_is_block(tag, tag_len)) {
                if (out_len && out[out_len - 1] != '\n')
                    out_len = append_char(out, out_cap, out_len, '\n');
            } else if (!in_script && !in_style) {
                pending_space = 1;
            }
            i = tag_end;
            continue;
        }
        if (in_script || in_style) continue;

        if (c == '&') {
            if (starts_with(src + i, "&nbsp;")) { c = ' '; i += 5; }
            else if (starts_with(src + i, "&amp;")) { c = '&'; i += 4; }
            else if (starts_with(src + i, "&lt;")) { c = '<'; i += 3; }
            else if (starts_with(src + i, "&gt;")) { c = '>'; i += 3; }
            else if (starts_with(src + i, "&quot;")) { c = '"'; i += 5; }
            else if (starts_with(src + i, "&#39;")) { c = '\''; i += 4; }
        }
        if (is_space(c)) {
            pending_space = 1;
            if (c == '\n' && out_len && out[out_len - 1] != '\n') {
                out_len = append_char(out, out_cap, out_len, '\n');
                pending_space = 0;
            }
            continue;
        }
        if (pending_space && out_len && out[out_len - 1] != '\n' && c != '\n')
            out_len = append_char(out, out_cap, out_len, ' ');
        pending_space = 0;
        out_len = append_char(out, out_cap, out_len, c);
    }
    while (out_len && (out[out_len - 1] == ' ' || out[out_len - 1] == '\n'))
        out_len--;
    out[out_len] = 0;
    return out_len;
}

static void report_error(const char *message) {
    puts(message);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        report_error("Usage: browser <http://host[:port]/path>");
        return 1;
    }

    const char *url = argv[1];
    if (starts_with(url, "https://")) {
        report_error("HTTPS is not supported yet: TLS is not implemented.");
        return 1;
    }

    const char *host_start = url;
    if (starts_with(url, "http://")) host_start += 7;
    else {
        report_error("Use an http:// address. HTTPS is not available yet.");
        return 1;
    }

    char host[128];
    char path[256];
    int host_len = 0;
    while (host_start[host_len] && host_start[host_len] != '/' &&
           host_start[host_len] != ':' && host_len < (int)sizeof(host) - 1) {
        host[host_len] = host_start[host_len];
        host_len++;
    }
    host[host_len] = 0;
    if (host_len == 0) {
        report_error("Invalid URL: host name is missing.");
        return 1;
    }

    int port = 80;
    const char *rest = host_start + host_len;
    if (*rest == ':') {
        port = atoi(rest + 1);
        while (*rest && *rest != '/') rest++;
        if (port < 1 || port > 65535) {
            report_error("Invalid URL: port must be between 1 and 65535.");
            return 1;
        }
    }
    if (*rest == '/') copy_part(path, sizeof(path), rest, (int)strlen(rest));
    else copy_part(path, sizeof(path), "/", 1);

    unsigned char ip[4];
    if (!dns_resolve(host, ip)) {
        report_error("Could not resolve the host. Check the network and address.");
        return 1;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0 || connect(fd, ip, (unsigned short)port) < 0) {
        if (fd >= 0) close(fd);
        report_error("Could not connect to the website.");
        return 1;
    }

    char request[512];
    int request_len = 0;
    const char *prefix = "GET ";
    for (int i = 0; prefix[i]; i++) request[request_len++] = prefix[i];
    for (int i = 0; path[i] && request_len < 400; i++)
        request[request_len++] = path[i];
    const char *middle = " HTTP/1.0\r\nHost: ";
    for (int i = 0; middle[i]; i++) request[request_len++] = middle[i];
    for (int i = 0; host[i] && request_len < 470; i++)
        request[request_len++] = host[i];
    const char *end = "\r\nUser-Agent: LambdaNexus/0.1\r\nConnection: close\r\n\r\n";
    for (int i = 0; end[i] && request_len < (int)sizeof(request) - 1; i++)
        request[request_len++] = end[i];

    if (send(fd, request, (unsigned long)request_len) != request_len) {
        close(fd);
        report_error("Failed to send the HTTP request.");
        return 1;
    }

    int received = 0;
    for (int attempt = 0; attempt < 100000 && received < RESPONSE_MAX - 1; attempt++) {
        long n = recv(fd, response + received,
                      (unsigned long)(RESPONSE_MAX - 1 - received));
        if (n == -2) { yield(); continue; }
        if (n <= 0) break;
        received += (int)n;
    }
    close(fd);
    response[received] = 0;
    if (received == 0) {
        report_error("The server returned no data.");
        return 1;
    }

    int body = contains(response, "\r\n\r\n");
    if (body >= 0) body += 4;
    else body = 0;
    int page_len = html_to_text(response + body, received - body, page, PAGE_MAX);
    if (page_len == 0) {
        report_error("The response did not contain readable page text.");
        return 1;
    }
    for (int offset = 0; offset < page_len;) {
        int chunk = page_len - offset;
        if (chunk > 512) chunk = 512;
        long written = write(STDOUT_FILENO, page + offset, (unsigned long)chunk);
        if (written <= 0) break;
        offset += (int)written;
    }
    return 0;
}
