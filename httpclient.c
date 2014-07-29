#include "httpclient.h"
#include <stdlib.h>
#include <wchar.h>
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


static bool dissect_address(const char* address, char* host, const size_t max_host_length, char* resource, const size_t max_resource_length)
{
  /* remove any protocol headers (f.e. "http://") before we search for first '/' */
  char* start_pos = strstr(address, "://");
  start_pos = ((start_pos == NULL) ? (char*) address : start_pos + 3);

  char* slash_pos = strchr(start_pos, '/');

  /* guards to avoid segfaults: */
  if (slash_pos == NULL)
    return false;

  char* addr_end = strchr(slash_pos, '\0');

  if (slash_pos == addr_end - 1)
    return false;

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
      break;
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
        printf("I IS %i\n", i);
        //memcpy(body, &http_resp[i], max_body_length - i);
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
    printf("0x%X ", msg[i]);
  printf("\n");
}  

http_ret_t http_request(const char* address, const http_req_t http_req, response_t* resp)
{
  int portno = 80;
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
    return HTTP_ERR_OPENING_SOCKET;

  struct hostent* server;

  char host_addr[128];
  char resource_addr[128];

  if (!dissect_address(address, host_addr, 128, resource_addr, 128))
    return HTTP_ERR_DISSECT_ADDR;


  server = gethostbyname(host_addr);

  if (server == NULL)
    return HTTP_ERR_NO_SUCH_HOST;

  struct sockaddr_in server_addr;

  bzero((char*) &server_addr, sizeof(server_addr));

  server_addr.sin_family = AF_INET;

  bcopy((char*) server->h_addr, (char*) &server_addr.sin_addr.s_addr, server->h_length);
  server_addr.sin_port = htons(portno);

  /* create TCP connection to host */
  if (connect(sock, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0)
    return HTTP_ERR_CONNECTING;

  char http_req_str[256];

  build_http_request(host_addr, resource_addr, http_req, http_req_str, 256);

  printf("REQ: \n%s\n", http_req_str);

  int len = write(sock, http_req_str, strlen(http_req_str));

  if (len < 0)
    return HTTP_ERR_WRITING;


  uint8_t resp_str[80000];
  uint32_t tot_len = 0;

  printf("RESPONSE: \n");
  do{
    bzero(&resp_str[tot_len], 80000 - tot_len);
    len = recv(sock, &resp_str[tot_len], 80000, 0);

    if (len <= 0)
      break;

  //  print(&resp_str[tot_len], len);
    tot_len += len;

  } while (len > 0);

  printf("\n\n.......................................................\n\n");
  long dest_len = 80000;
  uint8_t dest[80000];
  uint8_t* body = strstr(resp_str, "\r\n\r\n") + 4;

  long body_len = tot_len - (body - resp_str);

  z_stream defstream;
  defstream.zalloc = Z_NULL;
  defstream.zfree = Z_NULL;
  defstream.opaque = Z_NULL;
  print(&body[0], body_len);
  z_stream infstream;
  infstream.zalloc = Z_NULL;
  infstream.zfree = Z_NULL;
  infstream.opaque = Z_NULL;
  // setup "b" as the input and "c" as the compressed output
  infstream.avail_in = (uInt)((char*)defstream.next_out - (char*) body); // size of input
  infstream.next_in = (Bytef *)body; // input char array
  infstream.avail_out = (uInt)sizeof(dest); // size of output
  infstream.next_out = (Bytef *)dest; // output char array

  // the actual DE-compression work.
  inflateInit(&infstream);
  inflate(&infstream, Z_NO_FLUSH);
  inflateEnd(&infstream);
  //int compret = uncompress(dest, &dest_len, body, body_len);
  //printf("COMPRET: %i\n", compret);
  printf("---------------------------------------\n\n");
  printf("%s\n", dest);
  //print(body, 80000);
  close(sock);

  return HTTP_SUCCESS;

}




static void test_addr_dissect(char* addr)
{
  char host[128];
  char resource[128];
  printf("\n---------------------------------\n");
  if (dissect_address(addr, host, 128, resource, 128))
  {
    printf("ADDR:\t\t%s\nHOST: \t\t%s\nRESOURCE:\t%s\n", addr, host, resource);
  }
  else
  {
    printf("---- DISSECT FAILED: %s\n", addr);
  }
  fflush(stdout);
}


int main(void)
{
  //system("clear");
  char* addr1 = "https://api.stackexchange.com/2.2/search?order=desc&sort=activity&intitle=chmod&site=stackoverflow";
  char* addr2 = "folk.ntnu.no/trondesn/index.html";
  response_t resp;

  http_ret_t status = http_request(addr1, HTTP_REQ_GET, &resp);

  print_status(status);
}
