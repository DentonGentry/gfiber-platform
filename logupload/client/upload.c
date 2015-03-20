#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>

#include "upload.h"
#include "kvextract.h"
#include "utils.h"

#define DEVICE_KEY_PATH "/etc/ssl/private/device.key"
#define DEVICE_CERT_PATH "/etc/ssl/certs/device.pem"

#define FORM_DATA_SPLITTER_PREFIX "foo-splitter-"

#define CURL_CLEANUP_AND_RETURN \
  if (curl_handle) curl_easy_cleanup(curl_handle); \
  curl_global_cleanup(); \
  return -1

// Helper macro for setting a curl option and on error logging an error and
// returning -1. Thanks @drheld
#define SET_CURL_OPT(curl, option, param) \
  do { \
    CURLcode res = curl_easy_setopt(curl, option, param); \
    if (res != CURLE_OK) { \
      fprintf(stderr, "failed to set curl option %s: %s (%d)\n", \
             #option, curl_easy_strerror(res), res);    \
      if (curl_headers) curl_slist_free_all(curl_headers); \
    } \
  } while (0)

// If we ever change this to C++ pull in the arraysize macro instead.
#define ARRAYSIZE(x) (sizeof(x)/sizeof(x[0]))

struct getdatamem {
  char memory[4096]; // it'll be storing a URL, so this is big enough
  size_t used_size;
};

struct postdatamem {
  char content_type[128];
  char *blob;
  char data_prefix[128];
  char data_postfix[64];
  long prefix_offset;
  long prefix_length;
  long postfix_offset;
  long postfix_length;
  long blob_offset;
  long blob_length;
};

static size_t my_write_callback(void *contents, size_t size, size_t nmemb,
    void* userp) {
  size_t realsize = size * nmemb;
  struct getdatamem* memp = (struct getdatamem*) userp;
  if (realsize + memp->used_size > sizeof(memp->memory)) {
    fprintf(stderr, "curl sent back more data than fits in our buffer "
        " outsize=%zd usedsize=%zd\n", sizeof(memp->memory), realsize +
        memp->used_size);
    return 0;
  }
  memcpy(&(memp->memory[memp->used_size]), contents, realsize);
  memp->used_size += realsize;
  memp->memory[memp->used_size] = '\0';

  return realsize;
}

static size_t my_read_callback(char* buffer, size_t size, size_t nitems,
    void *instream) {
  size_t realsize = size * nitems;
  size_t xfer_size;
  struct postdatamem *postdata = (struct postdatamem*) instream;
  if (postdata->prefix_offset < postdata->prefix_length) {
    xfer_size = postdata->prefix_length - postdata->prefix_offset;
    if (xfer_size > realsize)
      xfer_size = realsize;
    memcpy(buffer, &(postdata->data_prefix[postdata->prefix_offset]),
        xfer_size);
    postdata->prefix_offset += xfer_size;
    realsize -= xfer_size;
    buffer = buffer + xfer_size;
  }
  if (realsize > 0 && postdata->blob_offset < postdata->blob_length) {
    xfer_size = postdata->blob_length - postdata->blob_offset;
    if (xfer_size > realsize)
      xfer_size = realsize;
    memcpy(buffer, &(postdata->blob[postdata->blob_offset]), xfer_size);
    postdata->blob_offset += xfer_size;
    realsize -= xfer_size;
    buffer = buffer + xfer_size;
  }
  if (realsize > 0 && postdata->postfix_offset < postdata->postfix_length) {
    xfer_size = postdata->postfix_length - postdata->postfix_offset;
    if (xfer_size > realsize)
      xfer_size = realsize;
    memcpy(buffer, &(postdata->data_postfix[postdata->postfix_offset]),
        xfer_size);
    postdata->postfix_offset += xfer_size;
    realsize -= xfer_size;
  }
  return (size * nitems) - realsize;
}

static int do_request_via_ipv(CURL *curl_handle, const char *server_url,
    struct getdatamem *getdata, struct postdatamem *postdata,
    int ipv, long *http_code) {
  struct curl_slist* curl_headers = NULL;
  struct curl_slist* new_headers = NULL;
  curl_easy_reset(curl_handle);
  SET_CURL_OPT(curl_handle, CURLOPT_URL, server_url);
  SET_CURL_OPT(curl_handle, CURLOPT_USERAGENT, "upload-logs");
  SET_CURL_OPT(curl_handle, CURLOPT_IPRESOLVE, ipv);
  SET_CURL_OPT(curl_handle, CURLOPT_FOLLOWLOCATION, 0L);
  SET_CURL_OPT(curl_handle, CURLOPT_SSL_VERIFYPEER, 1L);
  SET_CURL_OPT(curl_handle, CURLOPT_SSL_VERIFYHOST, 2L);
  if (path_exists(DEVICE_KEY_PATH))
    SET_CURL_OPT(curl_handle, CURLOPT_SSLKEY, DEVICE_KEY_PATH);
  if (path_exists(DEVICE_CERT_PATH))
    SET_CURL_OPT(curl_handle, CURLOPT_SSLCERT, DEVICE_CERT_PATH);

  // Log upload server doesn't like Expect: 100-continue
  // so remove that.
  new_headers = curl_slist_append(curl_headers, "Expect:");
  if (!new_headers) {
    fprintf(stderr, "failed setting up curl headers\n");
    return -1;
  } else
    curl_headers = new_headers;

  if (getdata) {
    SET_CURL_OPT(curl_handle, CURLOPT_HTTPGET, 1L);
    SET_CURL_OPT(curl_handle, CURLOPT_WRITEFUNCTION, my_write_callback);
    SET_CURL_OPT(curl_handle, CURLOPT_WRITEDATA, (void *)getdata);
  } else {
    SET_CURL_OPT(curl_handle, CURLOPT_POST, 1L);
    SET_CURL_OPT(curl_handle, CURLOPT_READFUNCTION, my_read_callback);
    SET_CURL_OPT(curl_handle, CURLOPT_READDATA, (void *)postdata);
    SET_CURL_OPT(curl_handle, CURLOPT_POSTFIELDSIZE,
        postdata->prefix_length + postdata->blob_length +
        postdata->postfix_length);
    new_headers = curl_slist_append(curl_headers, postdata->content_type);
    if (!new_headers) {
      fprintf(stderr, "failed setting up curl headers\n");
      curl_slist_free_all(curl_headers);
      return -1;
    } else
      curl_headers = new_headers;
    // If this is a retry we need to reset these values.
    postdata->prefix_offset = 0;
    postdata->blob_offset = 0;
    postdata->postfix_offset = 0;
  }
  SET_CURL_OPT(curl_handle, CURLOPT_HTTPHEADER, curl_headers);
  CURLcode curl_res = curl_easy_perform(curl_handle);
  curl_slist_free_all(curl_headers);
  curl_headers = NULL;
  *http_code = 500;
  curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, http_code);
  if (curl_res != CURLE_OK) {
    *http_code = 599;
    fprintf(stderr, "request failed: %s (%d)\n", curl_easy_strerror(curl_res),
        curl_res);
    return -1;
  } else {
    if (*http_code == 200) {
      if (getdata) {
        // Success for the GET case.
        return 0;
      } else {
        // This should not happen for POST, so it's failure.
        fprintf(stderr, "received a 200 response for a POST request\n");
        return -1;
      }
    } else if (*http_code == 302 && postdata) {
      // 302 is success for the post case.
      return 0;
    } else {
      // Failure due to some other HTTP error.
      return -1;
    }
  }
}

int upload_file(const char *server_url, const char* target_name, char* data,
    ssize_t len, struct kvpair* kvpairs_save) {
  char url[2048];
  CURL *curl_handle = NULL;
  int timeval;
  struct getdatamem getdata;
  int i;
  int resolvers[2] = { CURL_IPRESOLVE_V6, CURL_IPRESOLVE_V4 };
  const char *resolvers_str[2] = { "IPv6", "IPv4" };

  memset(&getdata, 0, sizeof(getdata));
  struct postdatamem postdata;
  memset(&postdata, 0, sizeof(postdata));
  // Strip any leading slashes from the target name
  while (target_name[0] == '/') {
    target_name++;
  }

  // Setup curl
  if (curl_global_init(CURL_GLOBAL_ALL)) {
    fprintf(stderr, "failed with curl_global_init\n");
    return -1;
  }

  for (i=0; i < (int)ARRAYSIZE(resolvers); ++i) {
    long http_code;
    struct kvpair* kvpairs = kvpairs_save;

    curl_handle = curl_easy_init();
    if (!curl_handle) {
      curl_global_cleanup();
      fprintf(stderr, "failed initializing curl\n");
      return -1;
    }

    // Construct the full URL
    snprintf(url, sizeof(url), "%s/upload/%s%s", server_url, target_name,
             (kvpairs != NULL) ? "?" : "");
    while (kvpairs != NULL) {
      // Now add the URL parameters
      size_t lim;
      char *url_end;

      char* encoded_key = curl_easy_escape(curl_handle, kvpairs->key, 0);
      if (!encoded_key) {
        fprintf(stderr, "failure in curl_easy_escape\n");
        CURL_CLEANUP_AND_RETURN;
      }

      char *encoded_value = curl_easy_escape(curl_handle, kvpairs->value, 0);
      if (!encoded_value) {
        fprintf(stderr, "failure in curl_easy_escape\n");
        CURL_CLEANUP_AND_RETURN;
      }

      url_end = url + strlen(url);
      lim = sizeof(url) - strlen(url);
      snprintf(url_end, lim, "%s=%s", encoded_key, encoded_value);

      curl_free(encoded_key);
      curl_free(encoded_value);
      kvpairs = kvpairs->next_pair;
      if (kvpairs)
        strncat(url, "&", sizeof(url));
    }

    http_code = 0;
    if (do_request_via_ipv(
            curl_handle, url, &getdata, NULL, resolvers[i], &http_code)) {
      fprintf(stderr, "Curl failed in GET %s\n", resolvers_str[i]);
      curl_easy_cleanup(curl_handle);
      continue;
    }

    timeval = (int) time(NULL);
    snprintf(postdata.content_type, sizeof(postdata.content_type),
             "Content-Type: multipart/form-data; boundary=%s-%d",
             FORM_DATA_SPLITTER_PREFIX, timeval);
    snprintf(postdata.data_prefix, sizeof(postdata.data_prefix),
             "--%s-%d\r\n"
             "Content-Disposition: form-data; name=\"file\"; "
             "filename=\"%s\"\r\n\r\n",
             FORM_DATA_SPLITTER_PREFIX, timeval, target_name);
    postdata.prefix_length = strlen(postdata.data_prefix);
    snprintf(postdata.data_postfix, sizeof(postdata.data_postfix),
             "\r\n--%s-%d--\r\n\r\n", FORM_DATA_SPLITTER_PREFIX, timeval);
    postdata.postfix_length = strlen(postdata.data_postfix);
    postdata.blob = data;
    postdata.blob_length = len;

    http_code = 0;
    if (do_request_via_ipv(
            curl_handle, getdata.memory, NULL, &postdata,
            resolvers[i], &http_code)) {
      curl_easy_cleanup(curl_handle);
      fprintf(stderr, "curl failed with %s http error: %ld\n",
              resolvers_str[i], http_code);
      continue;
    }

    // If we get here, that means the GET and POST both succeeded.
    break;
  }

  curl_easy_cleanup(curl_handle);
  curl_global_cleanup();
  return 0;
}
