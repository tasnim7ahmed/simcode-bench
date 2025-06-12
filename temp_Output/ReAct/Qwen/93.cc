#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-header.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpTosTtlExample");

class UdpReceiver : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("UdpReceiver")
                          .SetParent<Application>()
                          .AddConstructor<UdpReceiver>();
    return tid;
  }

  UdpReceiver() : m_socket(nullptr) {}
  virtual ~UdpReceiver() {}

protected:
  void DoInitialize() override
  {
    Application::DoInitialize();
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
    m_socket->Bind(local);
    m_socket->SetRecvCallback(MakeCallback(&UdpReceiver::Receive, this));
  }

  void DoDispose() override
  {
    m_socket = nullptr;
    Application::DoDispose();
  }

private:
  void StartApplication()
  {
    NS_LOG_DEBUG("Starting receiver application");
  }

  void StopApplication()
  {
    NS_LOG_DEBUG("Stopping receiver application");
  }

  void Receive(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      Ipv4Header ipHeader;
      packet->RemoveHeader(ipHeader);

      uint8_t tos = ipHeader.GetTos();
      uint8_t ttl = ipHeader.GetTtl();

      NS_LOG_UNCOND("Received packet size: " << packet->GetSize()
                                            << " TOS: " << (uint32_t)tos
                                            << " TTL: " << (uint32_t)ttl);
    }
  }

  Ptr<Socket> m_socket;
};

int main(int argc, char *argv[])
{
  uint32_t packetSize = 1024;
  uint32_t numPackets = 5;
  double interval = 1.0;
  uint8_t tos = 0;
  uint8_t ttl = 64;
  bool enableRecvTos = false;
  bool enableRecvTtl = false;

  CommandLine cmd(__FILE__);
  cmd.AddValue("PacketSize", "Size of each packet in bytes", packetSize);
  cmd.AddValue("NumPackets", "Number of packets to send", numPackets);
  cmd.AddValue("Interval", "Interval between packets in seconds", interval);
  cmd.AddValue("Tos", "IP_TOS value for packets", tos);
  cmd.AddValue("Ttl", "IP_TTL value for packets", ttl);
  cmd.AddValue("RecvTos", "Enable RECVTOS socket option at receiver", enableRecvTos);
  cmd.AddValue("RecvTtl", "Enable RECVTTL socket option at receiver", enableRecvTtl);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(2);

  CsmaHelper csma;
  csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(10000000)));
  csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

  NetDeviceContainer devices = csma.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  UdpReceiverHelper receiverHelper;
  ApplicationContainer sinkApps = receiverHelper.Install(nodes.Get(1));
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(10.0));

  uint16_t port = 9;
  OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
  onoff.SetConstantRate(DataRate(1000000 / interval));
  onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
  onoff.SetAttribute("MaxBytes", UintegerValue(packetSize * numPackets));

  ApplicationContainer apps = onoff.Install(nodes.Get(0));
  apps.Start(Seconds(0.1));
  apps.Stop(Seconds(10.0));

  Config::Set("/NodeList/0/ApplicationList/0/SocketIpTos", BooleanValue(true));
  Config::Set("/NodeList/0/ApplicationList/0/SocketIpTtl", BooleanValue(true));
  Config::Set("/NodeList/0/ApplicationList/0/IPTos", UintegerValue(tos));
  Config::Set("/NodeList/0/ApplicationList/0/IPTtl", UintegerValue(ttl));

  if (enableRecvTos)
  {
    Config::Set("/NodeList/1/ApplicationList/0/RecvTos", BooleanValue(true));
  }
  if (enableRecvTtl)
  {
    Config::Set("/NodeList/1/ApplicationList/0/RecvTtl", BooleanValue(true));
  }

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}