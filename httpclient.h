#ifndef _HTTP_CLIENT_H__
#define _HTTP_CLIENT_H__
#include <stdint.h>
#include <stdbool.h>
//#include "collection.h"

#define HTTP_REQ_GET      0
#define HTTP_REQ_HEAD     1
#define HTTP_REQ_POST     2
#define HTTP_REQ_PUT      3
#define HTTP_REQ_DELETE   4
#define HTTP_REQ_TRACE    5
#define HTTP_REQ_OPTIONS  6
#define HTTP_REQ_CONNECT  7
#define HTTP_REQ_PATCH    8


typedef int socket_t;
typedef uint32_t http_req_t;

typedef enum
{
  HTTP_SUCCESS = 0,
  HTTP_EMPTY_BODY,
  HTTP_ERR_OPENING_SOCKET,
  HTTP_ERR_DISSECT_ADDR,
  HTTP_ERR_NO_SUCH_HOST,
  HTTP_ERR_CONNECTING,
  HTTP_ERR_WRITING,
  HTTP_ERR_READING,
  HTTP_ERR_OUT_OF_MEM,
  HTTP_ERR_BAD_HEADER
} http_ret_t;


typedef struct 
{
  char* content_type;
  char* encoding;
  uint16_t status_code;
  char* status_text;
  char* redirect_addr;
} http_header_t;

typedef struct
{
  http_header_t* p_header;
  uint32_t length;
  char* contents;
  http_ret_t status;
} http_response_t;


/**
* @brief Make a HTTP request to the given server address
* @param address: address you want to request
* @param http_req: HTTP request type, any of the HTTP_REQ_* defines in the module
* @param resp: pointer to a reponse_t struct that will receive the response to the request
*/
http_response_t* http_request(char* const address, const http_req_t http_req);

/**
* @brief convenience function to dispose of the response struct
* @param resp: response struct to free
*/
//void http_resp_dispose(response_t* resp);



#endif /* _HTTP_CLIENT_H__ */
