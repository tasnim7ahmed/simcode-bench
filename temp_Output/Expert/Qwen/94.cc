#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Ipv6SocketOptionsSimulation");

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

void UdpReceiver::DoDispose()
{
  Application::DoDispose();
}

TypeId UdpReceiver::GetTypeId(void)
{
  static TypeId tid = TypeId("UdpReceiver")
                          .SetParent<Application>()
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
    m_socket->SetAttribute("RecvTClass", BooleanValue(true));
    m_socket->SetAttribute("RecvHopLimit", BooleanValue(true));
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
    socket->GetSockName(from);
    packet->RemoveHeader(ipHeader);

    uint8_t tclass = ipHeader.GetTrafficClass();
    uint8_t hoplimit = ipHeader.GetHopLimit();

    NS_LOG_UNCOND("Received packet size: " << packet->GetSize()
                  << " TCLASS: " << static_cast<uint32_t>(tclass)
                  << " HOPLIMIT: " << static_cast<uint32_t>(hoplimit));
  }
}

int main(int argc, char *argv[])
{
  uint32_t packetSize = 1024;
  uint32_t numPackets = 10;
  double interval = 1.0;
  uint8_t tclass = 0xb8; // Example TCLASS value
  uint8_t hoplimit = 64; // Default HOPLIMIT

  CommandLine cmd(__FILE__);
  cmd.AddValue("packetSize", "Size of packets in bytes", packetSize);
  cmd.AddValue("numPackets", "Number of packets to send", numPackets);
  cmd.AddValue("interval", "Interval between packets in seconds", interval);
  cmd.AddValue("tclass", "IPv6 Traffic Class value", tclass);
  cmd.AddValue("hoplimit", "IPv6 Hop Limit value", hoplimit);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv6AddressHelper address;
  address.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
  Ipv6InterfaceContainer interfaces = address.Assign(devices);

  // Receiver application
  UdpReceiverHelper receiverHelper;
  ApplicationContainer sinkApps = receiverHelper.Install(nodes.Get(1));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(10.0));

  // Sender socket setup
  TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
  Ptr<Socket> sourceSocket = Socket::CreateSocket(nodes.Get(0), tid);
  InetSocketAddress destAddr(interfaces.GetAddress(1, 1), 9); // IPv6 interface index 1
  sourceSocket->SetAllowBroadcast(false);
  sourceSocket->SetAttribute("IpTos", UintegerValue(tclass));
  sourceSocket->SetAttribute("UnicastHops", UintegerValue(hoplimit));

  // Sender packet generator
  Ptr<ConstantRandomVariable> intervalRv = CreateObject<ConstantRandomVariable>();
  intervalRv->SetAttribute("Constant", DoubleValue(interval));

  OnOffHelper onoff(sourceSocket->GetTypeId(), destAddr);
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate(1000000)));
  onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
  onoff.SetAttribute("NumberOfPackets", UintegerValue(numPackets));

  ApplicationContainer apps = onoff.Install(nodes.Get(0));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(10.0));

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}