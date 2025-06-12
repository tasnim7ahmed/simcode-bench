#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/log.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/packet.h"
#include "ns3/ipv4-header.h"
#include "ns3/udp-header.h"
#include "ns3/socket.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/address.h"
#include "ns3/inet-socket-address.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpLanExample");

// Receiver application to print received packets and their TOS/TTL values
class UdpReceiver : public Application
{
public:
  UdpReceiver();
  virtual ~UdpReceiver();

  static TypeId GetTypeId();
  void Setup(Address address, uint32_t port);

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

  void HandleRead(Ptr<Socket> socket);
  void PrintPacketInfo(Ptr<Packet> packet);

  Address m_localAddress;
  uint32_t m_localPort;
  Ptr<Socket> m_socket;
  bool m_running;
  Address m_fromAddress;
};

UdpReceiver::UdpReceiver() : m_running(false)
{
}

UdpReceiver::~UdpReceiver()
{
  m_socket = nullptr;
}

TypeId UdpReceiver::GetTypeId()
{
  static TypeId tid = TypeId("UdpReceiver")
                            .SetParent<Application>()
                            .SetGroupName("Applications")
                            .AddConstructor<UdpReceiver>();
  return tid;
}

void UdpReceiver::Setup(Address address, uint32_t port)
{
  m_localAddress = address;
  m_localPort = port;
}

void UdpReceiver::StartApplication(void)
{
  m_running = true;

  m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_localPort);
  m_socket->Bind(local);
  m_socket->SetRecvCallback(MakeCallback(&UdpReceiver::HandleRead, this));
}

void UdpReceiver::StopApplication(void)
{
  m_running = false;
  if (m_socket)
  {
    m_socket->Close();
  }
}

void UdpReceiver::HandleRead(Ptr<Socket> socket)
{
  Address fromAddress;
  Ptr<Packet> packet = socket->RecvFrom(fromAddress);
  m_fromAddress = fromAddress;
  if (packet->GetSize() > 0)
  {
    PrintPacketInfo(packet);
  }
}

void UdpReceiver::PrintPacketInfo(Ptr<Packet> packet)
{
  Ipv4Header ipHeader;
  packet->PeekHeader(ipHeader);
  NS_LOG_INFO("Received one packet:"
               << " Time = " << Simulator::Now().As(Time::S)
               << " Source = " << InetSocketAddress::ConvertFrom(m_fromAddress).GetIpv4()
               << " Destination = " << ipHeader.GetDestination()
               << " TOS = " << (uint32_t)ipHeader.GetTos()
               << " TTL = " << (uint32_t)ipHeader.GetTtl()
               << " Packet Size = " << packet->GetSize());
}

int main(int argc, char *argv[])
{
  LogComponentEnable("UdpLanExample", LOG_LEVEL_INFO);

  uint32_t packetSize = 1024;
  uint32_t numPackets = 10;
  double interval = 1.0;
  uint8_t tos = 0;
  uint8_t ttl = 64;
  bool recvTos = false;
  bool recvTtl = false;

  CommandLine cmd;
  cmd.AddValue("packetSize", "Size of the packets sent", packetSize);
  cmd.AddValue("numPackets", "Number of packets sent", numPackets);
  cmd.AddValue("interval", "Interval between packets", interval);
  cmd.AddValue("tos", "Type of Service (TOS) value", tos);
  cmd.AddValue("ttl", "Time To Live (TTL) value", ttl);
  cmd.AddValue("recvTos", "Receive TOS", recvTos);
  cmd.AddValue("recvTtl", "Receive TTL", recvTtl);

  cmd.Parse(argc, argv);

  NS_LOG_INFO("Packet size: " << packetSize);
  NS_LOG_INFO("Number of packets: " << numPackets);
  NS_LOG_INFO("Interval: " << interval);
  NS_LOG_INFO("TOS: " << (uint32_t)tos);
  NS_LOG_INFO("TTL: " << (uint32_t)ttl);
  NS_LOG_INFO("Receive TOS: " << recvTos);
  NS_LOG_INFO("Receive TTL: " << recvTtl);

  // Create nodes
  NodeContainer nodes;
  nodes.Create(2);

  // Create a CSMA channel
  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

  NetDeviceContainer devices = csma.Install(nodes);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Create UDP receiver application on node 1
  uint16_t port = 9;
  Ptr<Node> sinkNode = nodes.Get(1);
  UdpReceiver sinkApp;
  sinkApp.Setup(InetSocketAddress(Ipv4Address::GetAny(), port), port);

  ApplicationContainer sinkApps;
  Ptr<Application> appPtr = sinkNode->CreateObject<UdpReceiver>();
  sinkApps.Add(appPtr);
  Ptr<UdpReceiver> udpReceiver = DynamicCast<UdpReceiver>(appPtr);
  udpReceiver->Setup(InetSocketAddress(interfaces.GetAddress(1), port), port);
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(10.0));

  // Create UDP client application on node 0
  UdpClientHelper client(interfaces.GetAddress(1), port);
  client.SetAttribute("PacketSize", UintegerValue(packetSize));
  client.SetAttribute("MaxPackets", UintegerValue(numPackets));
  client.SetAttribute("Interval", TimeValue(Seconds(interval)));
  client.SetAttribute("Tos", UintegerValue(tos));
  client.SetAttribute("Ttl", UintegerValue(ttl));

  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}