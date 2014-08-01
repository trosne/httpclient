#include "httpclient.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>



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
}







int main()
{
  char addr[] = "www.example.com/";
  http_response_t* p_resp = http_request(addr, HTTP_REQ_GET);
  if (p_resp->status == HTTP_SUCCESS)
  {
    printf("RESPONSE FROM %s:\n%s\n", addr, p_resp->contents);
  }
  else
  {
    print_status(p_resp->status);
  }

  return 0;
}
