#include "imgur.h"
#include "base/file.h"
#include "base/wstring.h"
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

class MemFile : public File
{
  uint8* m_buf;
  uint32 m_pos;
  uint32 m_size;
  uint32 m_maxSize;
  void realloc(uint32 newSize)
  {
    while (m_maxSize < newSize)
      m_maxSize *= 2;
    uint8* temp = new uint8[m_maxSize];
    memcpy(temp, m_buf, m_size);
    delete[] m_buf;
    m_buf = temp;
  }
public:
  MemFile()
    : m_maxSize(64)
    , m_pos(0)
    , m_size(0)
  {
    m_buf = new uint8[m_maxSize];
  }
  ~MemFile()
  {
    delete[] m_buf;
  }

  uint8* buffer()
  {
    return m_buf;
  }
  uint32 size() const
  {
    return m_size;
  }

  bool readonly() const
  {
    return false;
  }

  uint8 getc()
  {
    if (m_pos >= m_size) return 0;
    return m_buf[m_pos++];
  }
  uint32 putc(uint8 c)
  {
    if (m_pos >= m_maxSize)
      realloc(m_pos + 1);
    m_buf[m_pos++] = c;
    if (m_pos > m_size)
      m_size = m_pos;
    return 1;
  }

  uint32 read(void* buf, uint32 count)
  {
    if (m_pos + count > m_size)
      count = m_size - m_pos;
    memcpy(buf, m_buf + m_pos, count);
    m_pos += count;
    return count;
  }
  uint32 write(void const* buf, uint32 count)
  {
    if (m_pos + count > m_maxSize)
      realloc(m_pos + count);
    memcpy(m_buf + m_pos, buf, count);
    m_pos += count;
    if (m_pos > m_size)
      m_size = m_pos;
    return count;
  }

  uint64 resize(uint64 newsize)
  {
    if (newsize > m_maxSize)
      realloc(newsize);
    return m_size = newsize;
  }

  void seek(uint64 pos, uint32 rel)
  {
    if (rel == SEEK_SET)
      m_pos = pos;
    else if (rel == SEEK_CUR)
      m_pos += pos;
    else if (rel == SEEK_END)
      m_pos = m_size + rel;
    if (m_pos >= m_size)
      m_pos = m_size;
  }
  uint64 tell() const
  {
    return m_pos;
  }

  bool eof() const
  {
    return m_pos >= m_size;
  }
};

static const char base64_e[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static void encode64(File* file, uint8 const* c, int n)
{
  uint32 val = 0;
  for (int i = 0; i < n; i++) val = (val << 8) | (*c++);
  for (int i = n; i < 3; i++) val <<= 8;
  char res[4];
  res[3] = base64_e[val & 0x3F], val >>= 6;
  res[2] = base64_e[val & 0x3F], val >>= 6;
  res[1] = base64_e[val & 0x3F], val >>= 6;
  res[0] = base64_e[val & 0x3F];
  if (n < 2) res[2] = '=';
  if (n < 3) res[3] = '=';
  file->write(res, 4);
}
static void base64_encode(File* file, uint8 const* ptr, int length)
{
  while (length >= 3)
  {
    encode64(file, ptr, 3);
    ptr += 3;
    length -= 3;
  }
  if (length)
    encode64(file, ptr, length);
}
static void urlencode(File* file, uint8 const* ptr, int length)
{
  for (int i = 0; i < length; i++)
  {
    if (isalnum(ptr[i]) || ptr[i] == '-' || ptr[i] == '.' || ptr[i] == '_' || ptr[i] == '~')
      file->putc(ptr[i]);
    else
      file->printf("%02X", int(ptr[i]));
  }
}

String uploadToImgur(Image* image)
{
  MemFile memRaw;
  image->writePNG(&memRaw);
  MemFile mem64;
  base64_encode(&mem64, memRaw.buffer(), memRaw.size());
  MemFile data;
  data.printf("key=b7668fe01959177&image=");
  urlencode(&data, mem64.buffer(), mem64.size());

  String response;
  HINTERNET hSession = WinHttpOpen(L"MuleView", WINHTTP_ACCESS_TYPE_NO_PROXY, NULL, NULL, 0);
  if (hSession)
  {
    HINTERNET hConnect = WinHttpConnect(hSession, L"api.imgur.com", INTERNET_DEFAULT_HTTP_PORT, 0);
    if (hConnect)
    {
      HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/2/upload.xml", L"HTTP/1.1",
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_BYPASS_PROXY_CACHE);
      if (hRequest)
      {
        BOOL result = WinHttpSendRequest(hRequest, NULL, 0, data.buffer(), data.size(), data.size(), NULL);
        if (result)
          result = WinHttpReceiveResponse(hRequest, NULL);
        DWORD avail;
        if (result)
          result = WinHttpQueryDataAvailable(hRequest, &avail);
        if (result)
        {
          char* ptr = new char[avail + 1];
          WinHttpReadData(hRequest, ptr, avail, &avail);
          ptr[avail] = 0;
          response = ptr;
          delete[] ptr;
        }
        WinHttpCloseHandle(hRequest);
      }
      WinHttpCloseHandle(hConnect);
    }
    WinHttpCloseHandle(hSession);
  }
  return response;
}
