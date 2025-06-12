#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpTosTtlExample");

class UdpReceiver : public Application
{
public:
  UdpReceiver();
  virtual ~UdpReceiver();

  static TypeId GetTypeId(void);
  void SetLocal(Address address);

protected:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

private:
  void HandleRead(Ptr<Socket> socket);

  Ptr<Socket> m_socket;
  Address m_local;
};

UdpReceiver::UdpReceiver()
    : m_socket(0)
{
}

UdpReceiver::~UdpReceiver()
{
  m_socket = nullptr;
}

TypeId UdpReceiver::GetTypeId(void)
{
  static TypeId tid = TypeId("UdpReceiver")
                          .SetParent<Application>()
                          .AddConstructor<UdpReceiver>();
  return tid;
}

void UdpReceiver::SetLocal(Address address)
{
  m_local = address;
}

void UdpReceiver::StartApplication(void)
{
  if (!m_socket)
  {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->Bind(m_local);
    m_socket->SetRecvCallback(MakeCallback(&UdpReceiver::HandleRead, this));
    if (InetSocketAddress::IsMatchingType(m_local))
    {
      NS_LOG_INFO("Starting UDP receiver on port " << InetSocketAddress(m_local).GetPort());
    }
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

void UdpReceiver::HandleRead(Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from)))
  {
    uint8_t tos = 0;
    uint8_t ttl = 0;

    SocketIpv4TosTag tosTag;
    SocketIpv4TtlTag ttlTag;

    if (packet->PeekPacketTag(tosTag))
    {
      tos = tosTag.GetTos();
    }

    if (packet->PeekPacketTag(ttlTag))
    {
      ttl = ttlTag.GetTtl();
    }

    NS_LOG_UNCOND("Received packet size: " << packet->GetSize() << " bytes, TOS: " << (uint32_t)tos << ", TTL: " << (uint32_t)ttl);
  }
}

int main(int argc, char *argv[])
{
  std::string phyType = "Csma";
  std::string dataMode = "SizeAndRate";
  std::string dataRate = "100Mbps";
  uint32_t packetSize = 1024;
  uint32_t numPackets = 5;
  double interval = 1.0;
  uint8_t ipTos = 0;
  uint8_t ipTtl = 64;
  bool enableRecvTos = false;
  bool enableRecvTtl = false;

  CommandLine cmd(__FILE__);
  cmd.AddValue("phyType", "Phy type: Csma", phyType);
  cmd.AddValue("dataRate", "Data rate for the link", dataRate);
  cmd.AddValue("packetSize", "Size of application packets sent", packetSize);
  cmd.AddValue("numPackets", "Number of packets to send", numPackets);
  cmd.AddValue("interval", "Interval between packets (seconds)", interval);
  cmd.AddValue("ipTos", "IP_TOS value for sender", ipTos);
  cmd.AddValue("ipTtl", "IP_TTL value for sender", ipTtl);
  cmd.AddValue("enableRecvTos", "Enable RECVTOS socket option at receiver", enableRecvTos);
  cmd.AddValue("enableRecvTtl", "Enable RECVTTL socket option at receiver", enableRecvTtl);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(dataRate)));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  uint16_t port = 9;

  // Receiver application
  UdpReceiverHelper receiverHelper(port);
  receiverHelper.Install(nodes.Get(1));

  // Sender application
  OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), port)));
  onoff.SetConstantRate(DataRate(dataRate), packetSize);
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
  onoff.SetAttribute("MaxBytes", UintegerValue(numPackets * packetSize));
  onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
  onoff.SetAttribute("StopTime", TimeValue(Seconds(1.0 + (numPackets - 1) * interval)));

  ApplicationContainer senderApp = onoff.Install(nodes.Get(0));

  // Set socket options for sender
  for (uint32_t i = 0; i < senderApp.GetN(); ++i)
  {
    Ptr<Socket> socket = DynamicCast<OnOffApplication>(senderApp.Get(i))->GetSocket();
    socket->SetIpTos(ipTos);
    socket->SetIpTtl(ipTtl);
  }

  // Configure socket options for receiver
  if (enableRecvTos || enableRecvTtl)
  {
    for (uint32_t i = 0; i < nodes.Get(1)->GetApplication(0)->GetSocketCount(); ++i)
    {
      Ptr<Socket> sock = nodes.Get(1)->GetApplication(0)->GetSocket(i);
      if (enableRecvTos)
        sock->SetRecvIpv4Tos(true);
      if (enableRecvTtl)
        sock->SetRecvIpv4Ttl(true);
    }
  }

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}