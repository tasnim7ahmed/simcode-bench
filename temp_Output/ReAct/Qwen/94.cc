#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpIpv6TClassHopLimitExample");

class UdpReceiver : public Application
{
public:
  UdpReceiver();
  virtual ~UdpReceiver();

  static TypeId GetTypeId(void);
  void DoDispose(void);

protected:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

private:
  void ReceivePacket(Ptr<Socket> socket);

  Ptr<Socket> m_socket;
};

UdpReceiver::UdpReceiver()
    : m_socket(0)
{
}

UdpReceiver::~UdpReceiver()
{
  m_socket = nullptr;
}

void UdpReceiver::DoDispose(void)
{
  Application::DoDispose();
}

TypeId UdpReceiver::GetTypeId(void)
{
  static TypeId tid = TypeId("UdpReceiver")
                          .SetParent<Application>()
                          .SetGroupName("Tutorial")
                          .AddConstructor<UdpReceiver>();
  return tid;
}

void UdpReceiver::StartApplication(void)
{
  if (!m_socket)
  {
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    m_socket = Socket::CreateSocket(GetNode(), tid);
    Inet6SocketAddress local = Inet6SocketAddress(Ipv6Address::GetAny(), 9);
    m_socket->Bind(local);
    m_socket->SetRecvCallback(MakeCallback(&UdpReceiver::ReceivePacket, this));
    m_socket->SetAttribute("RcvBufSize", UintegerValue(1 << 20)); // Enable large receive buffer

    // Enable RECVTCLASS and RECVHOPLIMIT options
    m_socket->SetIpRecvTClass(true);
    m_socket->SetIpRecvHopLimit(true);
  }
}

void UdpReceiver::StopApplication(void)
{
  if (m_socket)
  {
    m_socket->Close();
    m_socket = nullptr;
  }
}

void UdpReceiver::ReceivePacket(Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from)))
  {
    Ipv6Header ipHeader;
    socket->GetReceivedIpHeader(ipHeader);

    uint8_t tclass = ipHeader.GetTrafficClass();
    uint8_t hoplimit = ipHeader.GetHopLimit();

    NS_LOG_UNCOND("Received packet size: " << packet->GetSize()
                                          << " TCLASS: " << static_cast<uint32_t>(tclass)
                                          << " HOPLIMIT: " << static_cast<uint32_t>(hoplimit));
  }
}

int main(int argc, char *argv[])
{
  uint32_t packetSize = 512;
  uint32_t numPackets = 10;
  double interval = 1.0;
  uint8_t tclass = 0;
  uint8_t hoplimit = 64;

  CommandLine cmd(__FILE__);
  cmd.AddValue("packetSize", "Size of each UDP packet in bytes", packetSize);
  cmd.AddValue("numPackets", "Number of packets to send", numPackets);
  cmd.AddValue("interval", "Interval between packets in seconds", interval);
  cmd.AddValue("tclass", "IPv6 Traffic Class value", tclass);
  cmd.AddValue("hoplimit", "IPv6 Hop Limit value", hoplimit);
  cmd.Parse(argc, argv);

  Time interPacketInterval = Seconds(interval);

  NodeContainer nodes;
  nodes.Create(2);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv6AddressHelper address;
  Ipv6InterfaceContainer interfaces = address.Assign(devices);
  interfaces.SetForwarding(1, true);
  interfaces.SetDefaultRouteInAllNodes(1);

  UdpReceiverHelper receiverHelper;
  ApplicationContainer sinkApps = receiverHelper.Install(nodes.Get(1));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(10.0));

  TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
  Ptr<Socket> senderSocket = Socket::CreateSocket(nodes.Get(0), tid);

  senderSocket->SetAttribute("Icmpv6Redirect", BooleanValue(false));
  senderSocket->SetAttribute("Tclass", UintegerValue(tclass));
  senderSocket->SetAttribute("HopLimit", UintegerValue(hoplimit));

  InetSocketAddress destAddr(interfaces.GetAddress(1, 1), 9);
  destAddr.SetIpv6(interfaces.GetAddress(1, 1));
  senderSocket->Connect(destAddr);

  Simulator::Schedule(Seconds(1.0), &Socket::Send, senderSocket, Create<Packet>(packetSize), 0);
  for (uint32_t i = 1; i < numPackets; ++i)
  {
    Simulator::Schedule(Seconds(1.0 + i * interval), &Socket::Send, senderSocket, Create<Packet>(packetSize), 0);
  }

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}