#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/udp-client-server-helper.h"
#include <iostream>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocAodvSimulation");

uint32_t g_packetsSent = 0;
uint32_t g_packetsReceived = 0;
double g_totalDelay = 0.0;

void
TxAppPacket(Ptr<const Packet> packet)
{
  g_packetsSent++;
}

void
RxAppPacket(Ptr<const Socket> socket)
{
  Address from;
  Ptr<Packet> packet;
  while ((packet = socket->RecvFrom(from)))
    {
      g_packetsReceived++;
      Time recvTime = Simulator::Now();

      // Retrieve sending timestamp
      uint64_t sendTimeUs = 0;
      packet->PeekPacketTag(TimeTag(sendTimeUs));
      if (sendTimeUs > 0)
        {
          Time sendTime = MicroSeconds(sendTimeUs);
          g_totalDelay += (recvTime - sendTime).GetSeconds();
        }
    }
}

// Helper tag to carry sending time.
class TimeTag : public Tag
{
public:
  TimeTag() : m_timeUs(0) {}
  TimeTag(uint64_t t) : m_timeUs(t) {}
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("TimeTag")
      .SetParent<Tag>()
      .AddConstructor<TimeTag>();
    return tid;
  }
  virtual TypeId GetInstanceTypeId(void) const
  {
    return GetTypeId();
  }
  virtual void Serialize(TagBuffer i) const
  {
    i.WriteU64(m_timeUs);
  }
  virtual void Deserialize(TagBuffer i)
  {
    m_timeUs = i.ReadU64();
  }
  virtual uint32_t GetSerializedSize(void) const
  {
    return 8;
  }
  virtual void Print(std::ostream &os) const
  {
    os << "t=" << m_timeUs;
  }
  void SetTime(Time t) { m_timeUs = t.GetMicroSeconds(); }
  Time GetTime() const { return MicroSeconds(m_timeUs); }
private:
  uint64_t m_timeUs;
};

class CustomUdpApp : public Application
{
public:
  CustomUdpApp() {}
  virtual ~CustomUdpApp() { m_socket = 0; }

  void Setup(Address address, uint16_t port, uint32_t packetSize, uint32_t nPackets, double interval)
  {
    m_peerAddress = address;
    m_peerPort = port;
    m_packetSize = packetSize;
    m_nPackets = nPackets;
    m_interval = interval;
  }

protected:
  virtual void StartApplication()
  {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->Connect(InetSocketAddress(InetSocketAddress::ConvertFrom(m_peerAddress).GetIpv4(), m_peerPort));
    SendPacket();
  }

  virtual void StopApplication()
  {
    if (m_socket)
      {
        m_socket->Close();
      }
  }

  void SendPacket()
  {
    Ptr<Packet> packet = Create<Packet>(m_packetSize);
    TimeTag ttag;
    ttag.SetTime(Simulator::Now());
    packet->AddPacketTag(ttag);
    m_socket->Send(packet);
    g_packetsSent++;
    m_sent++;
    if (m_sent < m_nPackets)
      {
        Simulator::Schedule(Seconds(m_interval), &CustomUdpApp::SendPacket, this);
      }
  }

private:
  Ptr<Socket> m_socket;
  Address m_peerAddress;
  uint16_t m_peerPort;
  uint32_t m_packetSize;
  uint32_t m_nPackets;
  double m_interval;
  uint32_t m_sent{0};
};

int main(int argc, char *argv[])
{
  uint32_t numNodes = 10;
  double simTime = 30.0;
  double areaSize = 500.0;
  uint16_t port = 4000;
  uint32_t packetSize = 512;
  uint32_t packetsPerFlow = 40;
  double minStart = 1.0;
  double maxStart = 10.0;
  double interval = 0.5;

  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(numNodes);

  // Wi-Fi (Ad-hoc)
  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  wifiMac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

  // Mobility
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                          "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=10.0]"),
                          "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.3]"),
                          "PositionAllocator", PointerValue(
                            CreateObjectWithAttributes<RandomRectanglePositionAllocator>(
                            "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"),
                            "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"))));
  mobility.Install(nodes);

  // Internet stack + AODV
  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingHelper(aodv);
  stack.Install(nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.0.0", "255.255.0.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

  // UDP flows setup
  Ptr<UniformRandomVariable> randomSrc = CreateObject<UniformRandomVariable>();
  Ptr<UniformRandomVariable> randomDst = CreateObject<UniformRandomVariable>();
  Ptr<UniformRandomVariable> randomStart = CreateObject<UniformRandomVariable>();
  std::vector<Ptr<Socket>> recvSockets;

  // Assign UDP receivers to all nodes
  for (uint32_t i = 0; i < numNodes; ++i)
    {
      Ptr<Socket> recvSocket = Socket::CreateSocket(nodes.Get(i), UdpSocketFactory::GetTypeId());
      InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), port);
      recvSocket->Bind(local);
      recvSocket->SetRecvCallback(MakeCallback(&RxAppPacket));
      recvSockets.push_back(recvSocket);
    }

  // Setup applications
  for (uint32_t flow = 0; flow < numNodes; ++flow)
    {
      uint32_t srcIdx, dstIdx;
      do {
          srcIdx = randomSrc->GetInteger(0, numNodes - 1);
          dstIdx = randomDst->GetInteger(0, numNodes - 1);
      } while (srcIdx == dstIdx);

      Address dstAddress = InetSocketAddress(interfaces.GetAddress(dstIdx), port);

      Ptr<CustomUdpApp> app = CreateObject<CustomUdpApp>();
      app->Setup(dstAddress, port, packetSize, packetsPerFlow, interval);
      nodes.Get(srcIdx)->AddApplication(app);

      double appStart = randomStart->GetValue(minStart, maxStart);
      app->SetStartTime(Seconds(appStart));
      app->SetStopTime(Seconds(simTime));
    }

  // Enable pcap
  wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
  wifiPhy.EnablePcapAll("adhoc-aodv");

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();

  double pdr = g_packetsSent > 0 ? (double)g_packetsReceived / (double)g_packetsSent * 100.0 : 0.0;
  double avgDelay = g_packetsReceived > 0 ? (g_totalDelay / g_packetsReceived) : 0.0;

  std::cout << "Simulation Results\n";
  std::cout << "------------------\n";
  std::cout << "Packets Sent:     " << g_packetsSent << std::endl;
  std::cout << "Packets Received: " << g_packetsReceived << std::endl;
  std::cout << "Packet Delivery Ratio (PDR): " << pdr << " %" << std::endl;
  std::cout << "Average End-to-End Delay: " << avgDelay << " s" << std::endl;

  Simulator::Destroy();

  return 0;
}