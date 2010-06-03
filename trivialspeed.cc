#include <cstdio>
#include <unistd.h>
#include <cassert>
#include <memory>
#include <string>

#include <kchashdb.h>
#include <evhttp.h>

const char* TRIVIALSPEED_VERSION = "0.01";

using namespace std;

struct ts_ctx {
    kyotocabinet::HashDB db;
    std::string mimetype;
};

void main_request_handler(struct evhttp_request *r, void *args) {
    struct ts_ctx * ctx = reinterpret_cast<struct ts_ctx*>(args);

    /* check reqeust type. currently only suppoert GET */
    if (r->type != EVHTTP_REQ_GET) {
        fprintf(stderr, "INVALID METHOD: %d\n", r->type);
        evhttp_send_error(r, HTTP_BADREQUEST, "only support GET request!");
        return;
    }
    const char * path = evhttp_request_uri(r);
    std::auto_ptr<std::string> val_container(ctx->db.get(std::string(path)));
    std::string * val = val_container.get();
    if (!val) {
        // fprintf(stderr, "noto found: %s\n", path);
        evhttp_send_error(r, HTTP_NOTFOUND, "file not found");
        return;
    }

    struct evbuffer *evbuf = evbuffer_new();
    if (!evbuf) {
        fprintf(stderr, "[FATAL] failed to create response buffer\n");
        evhttp_send_error(r, HTTP_SERVUNAVAIL, "failed to create response buffer");
        return;
    }
    if (!ctx->mimetype.empty()) {
        evhttp_add_header(r->output_headers, "Content-Type", ctx->mimetype.c_str());
    }
    {
        std::ostringstream cl;
        cl << val->size();
        evhttp_add_header(r->output_headers, "Content-Length", cl.str().c_str());
    }
    evbuffer_add(evbuf, val->c_str(), val->size());
    evhttp_send_reply(r, HTTP_OK, "", evbuf);
    evbuffer_free(evbuf);
}

static void usage() {
    printf("Usage: trivialspeed dbpath\n");
    printf("  -a addr\n");
    printf("  -p port\n");
    printf("  -t timeout in seconds\n");
}

int main(int argc, char **argv) {
    std::string conf_addr("0");
    std::string conf_db;
    std::string conf_mimetype;
    int conf_port = 80;
    int conf_timeout = 8; // in seconds

    struct ts_ctx ctx;

    char ch;
    while ((ch = getopt(argc, argv, "a:p:t:m:")) != -1) {
        switch (ch) {
        case 'a':
            conf_addr = optarg;
            break;
        case 'p':
            assert(sscanf(optarg, "%d", &conf_port) == 1);
            break;
        case 't':
            assert(sscanf(optarg, "%d", &conf_timeout) == 1);
            break;
        case 'm':
            ctx.mimetype = optarg;
            break;
        default:
            usage();
            exit(1);
        }
    }
    if (optind >= argc) {
        usage();
        exit(1);
    }
    conf_db = argv[optind++];
    if (!ctx.db.open(conf_db, kyotocabinet::HashDB::OREADER)) {
        fprintf(stderr, "Cannot open database: %s\n", conf_db.c_str());
        exit(1);
    }

    printf("trivialspeed %s(libevent %s)\n", TRIVIALSPEED_VERSION, event_get_version());

    // main loop
    event_init();
    struct evhttp *httpd = evhttp_start(conf_addr.c_str(), conf_port);
    if (!httpd) {
        fprintf(stderr, "Cannot bind port: %s:%d\n", conf_addr.c_str(), conf_port);
        exit(1);
    }
    evhttp_set_timeout(httpd, conf_timeout);
    evhttp_set_gencb(httpd, main_request_handler, &ctx);
    event_dispatch();
    evhttp_free(httpd);

    // after use
    ctx.db.close();

    return 0;
}

