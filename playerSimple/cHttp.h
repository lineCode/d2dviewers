// cHttp.h
#pragma once
//{{{  includes
#include "cParsedUrl.h"
//}}}

class cHttp {
public:
  cHttp() : mContent(nullptr), mRedirectUrl(nullptr) {}
  //{{{
  ~cHttp() {

    if (mContent)
      vPortFree (mContent);

    if (mRedirectUrl)
      delete mRedirectUrl;
    }
  //}}}

  // gets
  //{{{
  int getResponse() {
    return mResponseCode;
    }
  //}}}
  //{{{
  const char* getRedirectedHost() {
    return mRedirectUrl ? mRedirectUrl->host : nullptr;
    }
  //}}}
  //{{{
  uint8_t* getContent() {
    return mContent;
    }
  //}}}
  //{{{
  int getContentSize() {
    return mContentSize;
      }
  //}}}
  //{{{
  uint8_t* getContentEnd() {
    return mContent + mContentSize;
    }
  //}}}
  //{{{
  const char* getErrorStr() {
    return mErrorStr;
    }
  //}}}

#ifdef WIN32
  //{{{
  int get (const char* host, const char* path) {
  // send http GET request to host, return response code

    //{{{  init
    mResponseCode = 0;

    mState = http_header;
    mParseHeaderState = http_parse_header_done;
    mChunked = 0;

    mContentLen = -1;
    mKeyStrLen = 0;
    mValueStrLen = 0;
    mOrigHost = host;

    mContentSize = 0;
    if (mContent) {
      vPortFree (mContent);
      mContent = nullptr;
      }
    //}}}
    //{{{  create webSocket from host, connect
    struct addrinfo hints;
    memset (&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;       // IPv4
    hints.ai_protocol = IPPROTO_TCP; // TCP
    hints.ai_socktype = SOCK_STREAM; // TCP so its SOCK_STREAM

    struct addrinfo* targetAddressInfo = NULL;
    unsigned long getAddrRes = getaddrinfo (host, NULL, &hints, &targetAddressInfo);
    if (getAddrRes != 0 || targetAddressInfo == NULL) {
      printf ("Could not resolve the Host Name\n");
      return 0;
      }

    // create webSocket
    struct sockaddr_in sockAddr;
    sockAddr.sin_addr = ((struct sockaddr_in*)targetAddressInfo->ai_addr)->sin_addr;
    sockAddr.sin_family = AF_INET; // IPv4
    sockAddr.sin_port = htons(80); // HTTP Port: 80

    // free targetAddressInfo from getaddrinfo
    freeaddrinfo (targetAddressInfo);

    // create socket
    unsigned int webSocket = (unsigned int)socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (webSocket == -1) {
      printf ("Creation of the Socket Failed\n");
      return 0;
      }

    // connection to webSocket
    if (connect (webSocket, (SOCKADDR*)&sockAddr, sizeof(sockAddr)) != 0) {
      closesocket (webSocket);
      printf ("Could not connect\n");
      return 0;
      }
    //}}}

    strcpy (mScratch, "GET /");
    strcat (mScratch, path);
    strcat (mScratch, " HTTP/1.1\r\nHost: ");
    strcat (mScratch, host);
    strcat (mScratch, "\r\n\r\n");
    int httpRequestStrLen = (int)strlen (mScratch);

    int sentBytes = (int)send (webSocket, mScratch, httpRequestStrLen, 0);
    if ((sentBytes < httpRequestStrLen) || (sentBytes == -1)) {
      closesocket (webSocket);
      printf ("Could not send the request to the Server\n");
      return 0;
      }

    char buffer[0x10000];
    int needMoreData = 1;
    while (needMoreData) {
      const char* bufferPtr = buffer;
      int bufferBytesReceived = recv (webSocket, buffer, sizeof(buffer), 0);
      if (bufferBytesReceived <= 0) {
        printf ("Error receiving data\n");
        closesocket (webSocket);
        return 0;
        }

      while (needMoreData && (bufferBytesReceived > 0)) {
        int bytesReceived;
        needMoreData = parseRecvData (bufferPtr, bufferBytesReceived, bytesReceived);
        bufferBytesReceived -= bytesReceived;
        bufferPtr += bytesReceived;
        }
      }

    if (mState == http_error)
      mErrorStr = "getHttp - error parsing data\n";

    closesocket (webSocket);
    return mResponseCode;
    }
  //}}}
#else
  //{{{
  void get (const char* host, const char* path) {
  // send http GET request to host, return context

    //{{{  init
    mResponseCode = 0;

    mState = http_header;
    mParseHeaderState = http_parse_header_done;
    mChunked = 0;

    mContentLen = -1;
    mKeyStrLen = 0;
    mValueStrLen = 0;
    mOrigHost = host;

    mContentSize = 0;
    if (mContent) {
      pvPortMalloc (mContent);
      mContent = nullptr;
      }
    //}}}

    if (netconn_gethostbyname (host, &mIpAddr) != ERR_OK) {
      //{{{  error return
      mErrorStr = getHostByNameError;
      return;
      }
      //}}}

    mConn = netconn_new (NETCONN_TCP);
    if (netconn_connect (mConn, &mIpAddr, 80) != ERR_OK){
      //{{{  error return
      mErrorStr = "getHttp - netconn_connect failed\n";
      return;
      }
      //}}}

    // use header buffer as send buffer
    strcpy (mScratch, "GET /");
    strcat (mScratch, path);
    strcat (mScratch, " HTTP/1.1\r\nHost: ");
    strcat (mScratch, host);
    strcat (mScratch, "\r\n\r\n");
    int httpRequestStrLen = (int)strlen (mScratch);
    if (netconn_write (mConn, mScratch, httpRequestStrLen, NETCONN_NOCOPY) != ERR_OK) {
      //{{{  error return
      mErrorStr = "getHttp - send failed\n";
      netconn_close (mConn);
      return;
      }
      //}}}

    struct netbuf* buf = NULL;
    bool needMoreData = true;
    while (needMoreData) {
      if (netconn_recv (mConn, &buf) != ERR_OK) {
        //{{{  error return;
        mErrorStr = "getHttp - netconn_recv failed\n";
        netconn_close (mConn);
        return;
        }
        //}}}

      char* bufferPtr;
      uint16_t bufferBytesReceived;
      netbuf_data (buf, (void**)(&bufferPtr), &bufferBytesReceived);

      while (needMoreData && (bufferBytesReceived > 0)) {
        int bytesReceived;
        needMoreData = parseRecvData (bufferPtr, bufferBytesReceived, &bytesReceived) != 0;
        bufferBytesReceived -= bytesReceived;
        bufferPtr += bytesReceived;
        }

      netbuf_delete (buf);
      }

    if (mState == http_error)
      mErrorStr = "getHttp - error parsing data\n";

    netconn_close (mConn);
    return;
    }
  //}}}
#endif

private:
  //{{{
  enum eState {
    http_header,
    http_chunk_header,
    http_chunk_data,
    http_raw_data,
    http_close,
    http_error,
    };
  //}}}
  //{{{
  enum eParseHeaderState {
    http_parse_header_done,
    http_parse_header_continue,
    http_parse_header_version_character,
    http_parse_header_code_character,
    http_parse_header_status_character,
    http_parse_header_key_character,
    http_parse_header_value_character,
    http_parse_header_store_keyvalue
    };
  //}}}
  //{{{
  const uint8_t kHeaderState[88] = {
  //  *    \t    \n   \r    ' '     ,     :   PAD
    0x80,    1, 0xC1, 0xC1,    1, 0x80, 0x80, 0xC1, // state 0: HTTP version
    0x81,    2, 0xC1, 0xC1,    2,    1,    1, 0xC1, // state 1: Response code
    0x82, 0x82,    4,    3, 0x82, 0x82, 0x82, 0xC1, // state 2: Response reason
    0xC1, 0xC1,    4, 0xC1, 0xC1, 0xC1, 0xC1, 0xC1, // state 3: HTTP version newline
    0x84, 0xC1, 0xC0,    5, 0xC1, 0xC1,    6, 0xC1, // state 4: Start of header field
    0xC1, 0xC1, 0xC0, 0xC1, 0xC1, 0xC1, 0xC1, 0xC1, // state 5: Last CR before end of header
    0x87,    6, 0xC1, 0xC1,    6, 0x87, 0x87, 0xC1, // state 6: leading whitespace before header value
    0x87, 0x87, 0xC4,   10, 0x87, 0x88, 0x87, 0xC1, // state 7: header field value
    0x87, 0x88,    6,    9, 0x88, 0x88, 0x87, 0xC1, // state 8: Split value field value
    0xC1, 0xC1,    6, 0xC1, 0xC1, 0xC1, 0xC1, 0xC1, // state 9: CR after split value field
    0xC1, 0xC1, 0xC4, 0xC1, 0xC1, 0xC1, 0xC1, 0xC1, // state 10:CR after header value
    };
  //}}}
  //{{{
  const uint8_t kChunkState[20] = {
  //  *    LF    CR    HEX
    0xC1, 0xC1, 0xC1,    1, // s0: initial hex char
    0xC1, 0xC1,    2, 0x81, // s1: additional hex chars, followed by CR
    0xC1, 0x83, 0xC1, 0xC1, // s2: trailing LF
    0xC1, 0xC1,    4, 0xC1, // s3: CR after chunk block
    0xC1, 0xC0, 0xC1, 0xC1, // s4: LF after chunk block
    };
  //}}}

  //{{{
  bool parseChunked (int& size, char ch) {
  // Parses the size out of a chunk-encoded HTTP response. Returns non-zero if it
  // needs more data. Retuns zero success or error. When error: size == -1 On
  // success, size = size of following chunk data excluding trailing \r\n. User is
  // expected to process or otherwise seek past chunk data up to the trailing \r\n.
  // The state parameter is used for internal state and should be
  // initialized to zero the first call.

    auto code = 0;
    switch (ch) {
      case '\n': code = 1; break;
      case '\r': code = 2; break;
      case '0': case '1': case '2': case '3':
      case '4': case '5': case '6': case '7':
      case '8': case '9': case 'a': case 'b':
      case 'c': case 'd': case 'e': case 'f':
      case 'A': case 'B': case 'C': case 'D':
      case 'E': case 'F': code = 3; break;
      }

    auto newstate = kChunkState [mParseHeaderState * 4 + code];
    mParseHeaderState = (eParseHeaderState)(newstate & 0xF);

    switch (newstate) {
      case 0xC0:
        return size != 0;

      case 0xC1: /* error */
        size = -1;
        return false;

      case 0x01: /* initial char */
        size = 0;
        /* fallthrough */
      case 0x81: /* size char */
        if (ch >= 'a')
          size = size * 16 + (ch - 'a' + 10);
        else if (ch >= 'A')
          size = size * 16 + (ch - 'A' + 10);
        else
          size = size * 16 + (ch - '0');
        break;

      case 0x83:
        return size == 0;
      }

    return true;
    }
  //}}}
  //{{{
  eParseHeaderState parseHeaderChar (char ch) {
  // Parses a single character of an HTTP header stream. The state parameter is
  // used as internal state and should be initialized to zero for the first call.
  // Return value is a value from the http_parse_header enuemeration specifying
  // the semantics of the character. If an error is encountered,
  // http_parse_header_done will be returned with a non-zero state parameter. On
  // success http_parse_header_done is returned with the state parameter set to zero.

    auto code = 0;
    switch (ch) {
      case '\t': code = 1; break;
      case '\n': code = 2; break;
      case '\r': code = 3; break;
      case  ' ': code = 4; break;
      case  ',': code = 5; break;
      case  ':': code = 6; break;
      }

    auto newstate = kHeaderState [mParseHeaderState * 8 + code];
    mParseHeaderState = (eParseHeaderState)(newstate & 0xF);

    switch (newstate) {
      case 0xC0: return http_parse_header_done;
      case 0xC1: return http_parse_header_done;
      case 0xC4: return http_parse_header_store_keyvalue;
      case 0x80: return http_parse_header_version_character;
      case 0x81: return http_parse_header_code_character;
      case 0x82: return http_parse_header_status_character;
      case 0x84: return http_parse_header_key_character;
      case 0x87: return http_parse_header_value_character;
      case 0x88: return http_parse_header_value_character;
      }

    return http_parse_header_continue;
    }
  //}}}
  //{{{
  bool parseRecvData (const char* data, int len, int& read) {

    auto initial_size = len;
    while (len) {
      switch (mState) {
        case http_header:
          switch (parseHeaderChar (*data)) {
            case http_parse_header_code_character:
              //{{{  code char
              mResponseCode = mResponseCode * 10 + *data - '0';
              break;
              //}}}
            case http_parse_header_done:
              //{{{  code done
              if (mParseHeaderState != 0)
                mState = http_error;

              else if (mChunked) {
                mContentLen = 0;
                mState = http_chunk_header;
                }

              else if (mContentLen == 0)
                mState = http_close;

              else if (mContentLen > 0)
                mState = http_raw_data;

              else
                mState = http_error;

              break;
              //}}}
            case http_parse_header_key_character:
              //{{{  header key char
              mScratch [mKeyStrLen] = tolower((uint8_t)(*data));
              mKeyStrLen++;
              break;
              //}}}
            case http_parse_header_value_character:
              //{{{  header value char
              mScratch [mKeyStrLen + mValueStrLen] = *data;
              mValueStrLen++;
              break;
              //}}}
            case http_parse_header_store_keyvalue: {
              //{{{  key value done
              if ((mKeyStrLen == 17) &&
                  (strncmp (mScratch, "transfer-encoding", mKeyStrLen) == 0))
                mChunked =
                  (mValueStrLen == 7) &&
                  (strncmp (mScratch + mKeyStrLen, "chunked", mValueStrLen) == 0);

              else if ((mKeyStrLen == 14) &&
                       (strncmp (mScratch, "content-length", mKeyStrLen) == 0)) {
                mContentLen = 0;
                for (int ii = mKeyStrLen, end = mKeyStrLen + mValueStrLen; ii != end; ++ii)
                  mContentLen = mContentLen * 10 + mScratch[ii] - '0';
                mContent = (uint8_t*)pvPortMalloc (mContentLen);
                }
              else if ((mKeyStrLen == 8) &&
                       (strncmp (mScratch, "location", mKeyStrLen) == 0)) {
                if (!mRedirectUrl)
                  mRedirectUrl = new cParsedUrl();
                mRedirectUrl->parseUrl (mScratch + mKeyStrLen, mValueStrLen);
                }
              mKeyStrLen = 0;
              mValueStrLen = 0;
              break;
              }
              //}}}
            }
          --len;
          ++data;
          break;

        //{{{
        case http_chunk_header:

          if (!parseChunked (mContentLen, *data)) {
            if (mContentLen == -1)
              mState = http_error;
            else if (mContentLen == 0)
              mState = http_close;
            else
              mState = http_chunk_data;
            }

          --len;
          ++data;
          break;
        //}}}
        //{{{
        case http_chunk_data: {
          int chunksize = (len < mContentLen) ? len : mContentLen;

          if (mContent) {
            memcpy (mContent + mContentSize, data, chunksize);
            mContentSize += chunksize;
            }

          mContentLen -= chunksize;
          len -= chunksize;
          data += chunksize;

          if (mContentLen == 0) {
            mContentLen = 1;
            mState = http_chunk_header;
            }
          }
          break;
        //}}}
        //{{{
        case http_raw_data: {
          int chunksize = (len < mContentLen) ? len : mContentLen;

          if (mContent) {
            memcpy (mContent + mContentSize, data, chunksize);
            mContentSize += chunksize;
            }
          mContentLen -= chunksize;
          len -= chunksize;
          data += chunksize;

          if (mContentLen == 0)
            mState = http_close;
          }
          break;
        //}}}
        case http_close:
        case http_error:
          break;
        }

      if (mState == http_error || mState == http_close) {
        read = initial_size - len;
        return false;
        }
      }

    read = initial_size - len;
    return true;
    }
  //}}}

  //{{{  vars
  int mResponseCode;
  eState mState;
  eParseHeaderState mParseHeaderState;
  int mChunked;

  int mKeyStrLen;
  int mValueStrLen;
  int mContentLen;
  char mScratch[256];

  int mContentSize;
  uint8_t* mContent;
  const char* mErrorStr;

  const char* mOrigHost;
  cParsedUrl* mRedirectUrl;
  //}}}
#ifndef WIN32
  ip_addr_t mIpAddr;
  struct netconn* mConn;
#endif
  };
