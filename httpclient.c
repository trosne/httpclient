#include "httpclient.h"
#include <stdlib.h>
#include <zlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>




#define HTTP_1_1_STR "HTTP/1.1"

static const char* HTTP_REQUESTS[] =
{
  "GET",
  "HEAD",
  "POST",
  "PUT",
  "DELETE",
  "TRACE",
  "OPTIONS",
  "CONNECT",
  "PATCH"
};


static void word_to_string(const char* const word, char** str)
{
  uint32_t i = 0;
  while (word[i] == ' ' || word[i] == '\r' || word[i] == '\n' || word[i] == '\0')
    ++i;

  uint32_t start = i;

  while (word[i] != ' ' && word[i] != '\r' && word[i] != '\n')
    ++i;
  
  *str = (char*) malloc(i - start);
  memcpy(*str, &word[start], i - start);
}

static bool dissect_address(const char* address, char* host, const size_t max_host_length, char* resource, const size_t max_resource_length)
{
  /* remove any protocol headers (f.e. "http://") before we search for first '/' */
  char* start_pos = strstr(address, "://");
  start_pos = ((start_pos == NULL) ? (char*) address : start_pos + 3);

  char* slash_pos = strchr(start_pos, '/');

  /* no resource, send request to index.html*/
  if (slash_pos == NULL || slash_pos[1] == '\0')
  {
    strcpy(host, start_pos);

    /* remove slash from host if any */
    if (slash_pos != NULL)
      strchr(host, '/')[0] = '\0';

    strcpy(resource, "/index.html");
    return true;
  }

  char* addr_end = strchr(slash_pos, '\0');

  if (start_pos >= slash_pos - 1)
    return false;

  if (slash_pos - start_pos > max_host_length)
    return false;

  if (addr_end - slash_pos > max_resource_length)
    return false;

  strcpy(host, start_pos);
  host[slash_pos - start_pos] = '\0';
  strcpy(resource, slash_pos);
  resource[addr_end - slash_pos] = '\0';

  return true;
}

static void print_status(http_ret_t status)
{
  switch (status)
  {
    case HTTP_SUCCESS:
      printf("SUCCESS\n");
      return;
    case HTTP_ERR_OPENING_SOCKET:
      printf("ERROR: OPENING SOCKET\n");
      break;
    case HTTP_ERR_DISSECT_ADDR:
      printf("ERROR: DISSECTING ADDRESS\n");
      break;
    case HTTP_ERR_NO_SUCH_HOST:
      printf("ERROR: NO SUCH HOST\n");
      break;
    case HTTP_ERR_CONNECTING:
      printf("ERROR: CONNECTING TO HOST\n");
      break;
    case HTTP_ERR_WRITING:
      printf("ERROR: WRITING TO SOCKET\n");
      break;
    case HTTP_ERR_READING:
      printf("ERROR: READING FROM SOCKET\n");
      break;
    default:
      printf("ERROR: UNKNOWN STATUS CODE\n");
  }
  printf("errno: %s\n", strerror(errno));
}

static bool build_http_request(const char* host, const char* resource, const http_req_t http_req, char* request, const size_t max_req_size)
{
  sprintf(request, "%s %s %s\r\nHost: %s \r\n\r\n", 
      HTTP_REQUESTS[http_req],
      resource,
      HTTP_1_1_STR,
      host);
  return true;
}

static bool http_body_get(const char* http_resp, char* body, size_t max_body_length)
{

  uint32_t i, j = 0;
  char matchseq[] = {'\r', '\n', '\r', '\n'};

  for (i = 0; i < 80000; ++i)
  {
    if (http_resp[i] == matchseq[j])
    {
      ++j;
      if (j == sizeof(matchseq))
      {
        body = (char*) &http_resp[i];
        return true;
      }
    }
    else
    {
      j = 0;
    }
  }
  return false;
}

static void print(uint8_t* msg, uint32_t len)
{
  printf("-----------------\n");
  uint32_t i = 0;
  for (; i < len; ++i)
    printf("%2X ", msg[i]);
  printf("\n");
}  

static void socket_set_timeout(socket_t socket, uint32_t seconds, uint32_t usec)
{
  struct timeval tv;

  tv.tv_sec = seconds;
  tv.tv_usec = usec;

  setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));
}

static socket_t socket_open(struct hostent* host, int portno)
{
  socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
  
  if (sock < 0)
    return -1;

  struct sockaddr_in server_addr;

  memset((uint8_t*) &server_addr, 0, sizeof(server_addr));

  server_addr.sin_family = AF_INET;

  memcpy((uint8_t*) &server_addr.sin_addr.s_addr, (uint8_t*) host->h_addr, host->h_length);

  server_addr.sin_port = htons(portno);

  /* create TCP connection to host */
  int res =  connect(sock, (struct sockaddr*) &server_addr, sizeof(server_addr));
  if (res < 0)
  {
    return -1;
  }

  return sock;
} 

static void socket_close(socket_t socket)
{
  if (socket >= 0)
    close(socket);
}

static void dissect_header(char* data, http_response_t* p_resp)
{
  p_resp->p_header = (http_header_t*) malloc(sizeof(http_header_t));

  char* content = data;
  const char* header_end = strstr(content, "\r\n\r\n");
  
  content = strchr(data, ' ');
  if (content == NULL)
    return;
  
  p_resp->p_header->status_code = atoi(content);
  p_resp->p_header->content_type = NULL;
  p_resp->p_header->encoding = NULL;

  while (content <= header_end)
  {
    if (content == NULL)
      return;
    content = strstr(content, "\r\n");
    if (content == NULL)
      return;
    content += 2;

    if (strstr(content, "Content-Type:") == content)
    {
      content = strchr(content, ' ');
      word_to_string(content, &p_resp->p_header->content_type);
    }
    else if (strstr(content, "Content-Encoding:") == content)
    {
      content = strchr(content, ' ');
      word_to_string(content, &p_resp->p_header->encoding);
    }
  }
}
 
http_response_t* http_request(const char* address, const http_req_t http_req)
{
  http_response_t* p_resp = (http_response_t*) malloc(sizeof(http_response_t));
  memset(p_resp, 0, sizeof(http_response_t));
  

  int portno = 80;
  struct hostent* server;

  char host_addr[256];
  char resource_addr[256];

  if (!dissect_address(address, host_addr, 256, resource_addr, 256))
  {
    p_resp->status = HTTP_ERR_DISSECT_ADDR;
    return p_resp;
  }

  /* do DNS lookup */
  server = gethostbyname(host_addr);

  if (server == NULL)
  {
    p_resp->status = HTTP_ERR_NO_SUCH_HOST;
    return p_resp;
  }
  
  /* open socket to host */
  socket_t sock = socket_open(server, portno);

  if (sock < 0)
  {
    p_resp->status = HTTP_ERR_OPENING_SOCKET;
    return p_resp;
  }

  /* Default timeout is too long */
  socket_set_timeout(sock, 0, 500000);

  char http_req_str[512];

  build_http_request(host_addr, resource_addr, http_req, http_req_str, 256);

  /* send http request */
  int len = write(sock, http_req_str, strlen(http_req_str));

  if (len < 0)
  {
    p_resp->status = HTTP_ERR_WRITING;
    return p_resp;
  }

  uint8_t resp_str[80000];
  uint32_t tot_len = 0;
  uint32_t cycles = 0;
  do{
    memset(&resp_str[tot_len], 0, 80000 - tot_len);
    len = recv(sock, &resp_str[tot_len], 80000, 0);

    if (len <= 0 && cycles > 0)
      break;

    tot_len += len;
    ++cycles;

  } while (true);

  /* header ends with an empty line */
  uint8_t* body = strstr(resp_str, "\r\n\r\n") + 4;
  uint32_t header_len = (body - resp_str);
  uint32_t body_len = (tot_len - header_len);

  /* place data in response header */
  dissect_header(resp_str, p_resp);
 
  /* if contents are compressed, uncompress it before placing it in the struct */
  if (p_resp->p_header->encoding != NULL && strstr(p_resp->p_header->encoding, "gzip") != NULL)
  {
    /* content length is always stored in the 4 last bytes */
    uint32_t content_len;
    memcpy(&content_len, resp_str + tot_len - 4, 4);

    p_resp->contents = (char*) malloc(content_len * 2);
    
    z_stream infstream;
    infstream.avail_in = (uInt)(body_len);
    infstream.next_in = (Bytef *)body; 
    infstream.avail_out = (uInt)content_len;
    infstream.next_out = (Bytef *)p_resp->contents;

    // the actual DE-compression work.
    inflateInit2(&infstream, 16 + MAX_WBITS);
    inflate(&infstream, Z_NO_FLUSH);
    inflateEnd(&infstream);

    p_resp->length = content_len;
  }
  else if (body_len > 0)
  {
    p_resp->contents = (char*) malloc(body_len);
    memcpy(p_resp->contents, body, body_len);
    p_resp->length = body_len;
  }
  else
  {
    p_resp->status = HTTP_ERR_READING;
    return p_resp;
  }

  socket_close(sock);

  return p_resp;
}
