#include "StormWebrtcClient.h"

#include <vector>

#ifdef EMSCRIPTEN
#include <emscripten/html5.h>
#include <emscripten/emscripten.h>
#else
#define EM_ASM_ARGS(...)
#endif

static bool s_StormWebrtcInit = false;
struct StormWebrtcAllocatorData
{
  StormWebrtcAllocatorData()
  {
    m_NextIndex = 0;
    s_StormWebrtcInit = true;
  }

  ~StormWebrtcAllocatorData()
  {
    s_StormWebrtcInit = false;
  }

  int m_NextIndex;
  std::vector<int> m_StormWebrtcIdAllocator;
};

StormWebrtcAllocatorData s_StormWebrtcAllocInfo;
std::vector<StormWebrtcClient *> s_StormWebrtcs;

extern "C"
{
  void HandleStormWebrtcConnect(int index)
  {
    s_StormWebrtcs[index]->SetConnected(true);
  }

  void HandleStormWebrtcMessage(int index, int stream, void * data, int length)
  {
    s_StormWebrtcs[index]->GotMessage(stream, data, length);
  }

  void HandleStormWebrtcDisconnect(int index)
  {
    s_StormWebrtcs[index]->SetConnected(false);
  }
}

#define INVALID_SOCKET -1

StormWebrtcClient::StormWebrtcClient(const StormWebrtcClientChannelList & in_channels, const StormWebrtcClientChannelList & out_channels) :
  m_Socket((int)INVALID_SOCKET),
  m_Connected(false),
  m_Connecting(false),
  m_InChannels(in_channels),
  m_OutChannels(out_channels)
{

}

StormWebrtcClient::StormWebrtcClient(const StormWebrtcClientChannelList & in_channels, const StormWebrtcClientChannelList & out_channels, const char * ipaddr, int port, const char * fingerprint) :
  StormWebrtcClient(in_channels, out_channels)
{
  StartConnect(ipaddr, port, fingerprint);
}

StormWebrtcClient::~StormWebrtcClient()
{
  Close();
}

StormWebrtcClient::StormWebrtcClient(StormWebrtcClient && rhs) noexcept
{
  m_Socket = rhs.m_Socket;
  m_Connected = rhs.m_Connected;
  m_Connecting = rhs.m_Connecting;

  rhs.m_Socket = (int)INVALID_SOCKET;
  rhs.m_Connected = false;
  rhs.m_Connecting = false;
}

StormWebrtcClient & StormWebrtcClient::operator = (StormWebrtcClient && rhs) noexcept
{
  Close();

  m_Socket = rhs.m_Socket;
  m_Connected = rhs.m_Connected;
  m_Connecting = rhs.m_Connecting;

  rhs.m_Socket = (int)INVALID_SOCKET;
  rhs.m_Connected = false;
  rhs.m_Connecting = false;
  return *this;
}

void StormWebrtcClient::StartConnect(const char * ipaddr, int port, const char * fingerprint)
{
  Close();

  if (s_StormWebrtcInit == false)
  {
    return;
  }

  if (s_StormWebrtcAllocInfo.m_StormWebrtcIdAllocator.size() > 0)
  {
    m_Socket = s_StormWebrtcAllocInfo.m_StormWebrtcIdAllocator.back();
    s_StormWebrtcAllocInfo.m_StormWebrtcIdAllocator.pop_back();
  }
  else
  {
    m_Socket = s_StormWebrtcAllocInfo.m_NextIndex;
    s_StormWebrtcAllocInfo.m_NextIndex++;
  }

  while ((int)s_StormWebrtcs.size() <= m_Socket)
  {
    s_StormWebrtcs.push_back(nullptr);
  }

  s_StormWebrtcs[m_Socket] = this;
  EM_ASM_ARGS({ StormWebrtcCreateConnection($0, $1, $2, $3, $4, $5, $6, $7); }, m_Socket, ipaddr, port, m_InChannels.data(), m_InChannels.size(), m_OutChannels.data(), m_OutChannels.size());
  m_Connecting = true;
}

bool StormWebrtcClient::IsConnected()
{
  return m_Connected;
}

bool StormWebrtcClient::IsConnecting()
{
  return m_Connecting;
}

void StormWebrtcClient::Close()
{
  m_Connected = false;
  m_Connecting = false;

  if (s_StormWebrtcInit == false)
  {
    return;
  }

  if (m_Socket == (int)INVALID_SOCKET)
  {
    return;
  }

  s_StormWebrtcs[m_Socket] = nullptr;
  s_StormWebrtcAllocInfo.m_StormWebrtcIdAllocator.push_back(m_Socket);
  EM_ASM_ARGS({ StormWebrtcDestroyConnection($0); }, m_Socket);

  m_Socket = (int)INVALID_SOCKET;
}


void StormWebrtcClient::SendPacket(int stream, const void * data, std::size_t data_len)
{
  if (m_Connected == false)
  {
    return;
  }

  EM_ASM_ARGS({ StormWebrtcSendBinaryMessage($0, $1, $2, $3); }, m_Socket, stream, data, data_len);
}

bool StormWebrtcClient::PollPacket(StormWebrtcPacket & out_packet)
{
  if (m_PendingPackets.size())
  {
    return false;
  }

  out_packet = std::move(m_PendingPackets.front());
  m_PendingPackets.pop();
  return true;
}

void StormWebrtcClient::SetConnected(bool connected)
{
  m_Connected = connected;
  m_Connecting = false;
}

void StormWebrtcClient::GotMessage(int stream, void * data, int length)
{
  auto ptr = std::unique_ptr<uint8_t[], StormWebrtcPacketDeleter>((uint8_t *)data);
  m_PendingPackets.emplace(StormWebrtcPacket{ std::move(ptr), length, stream });
}

