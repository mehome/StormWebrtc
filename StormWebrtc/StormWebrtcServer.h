#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct StormWebrtcConnectionHandle
{
  StormWebrtcConnectionHandle() {}
  StormWebrtcConnectionHandle(uint16_t slot_id, uint16_t generation) : m_SlotId(slot_id), m_Generation(generation) {}

  uint16_t m_SlotId = 0xFFFF;
  uint16_t m_Generation = 0xFFFF;
};

enum class StormWebrtcEventType
{
  kConnected,
  kDisconnected,
  kData
};

enum StormWebrtcStreamType
{
  kReliable,
  kUnreliable,
};

struct StormWebrtcEvent
{
  StormWebrtcEventType m_Type;
  StormWebrtcConnectionHandle m_ConnectionHandle;
  uint32_t m_RemoteAddr;
  uint16_t m_RemotePort;
  std::unique_ptr<uint8_t[]> m_Buffer;
  uint16_t m_DataSize;
  uint16_t m_StreamIndex;
};

struct StormWebrtcServerSettings
{
  uint16_t m_Port;
  uint16_t m_MaxConnections;
  const char * m_KeyFile;
  const char * m_CertFile;

  std::vector<StormWebrtcStreamType> m_InStreams;
  std::vector<StormWebrtcStreamType> m_OutStreams;
};

class StormWebrtcServer
{
public:

  virtual ~StormWebrtcServer();

  virtual void Update() = 0;
  virtual bool PollEvent(StormWebrtcEvent & outp_event) = 0;
  virtual void SendPacket(const StormWebrtcConnectionHandle & handle, const void * data, std::size_t length, std::size_t stream) = 0;
  virtual void ForceDisconnect(const StormWebrtcConnectionHandle & handle) = 0;
};

void StormWebrtcStaticInit();
void StormWebrtcStaticCleanup();

std::unique_ptr<StormWebrtcServer> CreateStormWebrtcServer(const StormWebrtcServerSettings & settings);
