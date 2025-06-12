#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/udp-server-client-helper.h"
#include "ns3/application-container.h"
#include "ns3/packet-sink-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpBroadcastSimulation");

class UdpReceiver : public Application
{
public:
  static TypeId GetTypeId();
  UdpReceiver();
  virtual ~UdpReceiver();

protected:
  void DoStart() override;
  void DoDispose() override;

private:
  void HandleRead(Ptr<Socket> socket);
  Ptr<Socket> m_socket;
};

TypeId
UdpReceiver::GetTypeId()
{
  static TypeId tid = TypeId("UdpReceiver")
                          .SetParent<Application>()
                          .AddConstructor<UdpReceiver>();
  return tid;
}

UdpReceiver::UdpReceiver()
    : m_socket(nullptr)
{
}

UdpReceiver::~UdpReceiver()
{
}

void
UdpReceiver::DoStart()
{
  m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
  m_socket->Bind(local);
  m_socket->SetRecvCallback(MakeCallback(&UdpReceiver::HandleRead, this));
  Application::DoStart();
}

void
UdpReceiver::DoDispose()
{
  m_socket->Close();
  m_socket = nullptr;
  Application::DoDispose();
}

void
UdpReceiver::HandleRead(Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from)))
  {
    NS_LOG_INFO(Simulator::Now().As(Time::S) << " Received packet size: " << packet->GetSize() << " from " << from);
  }
}

int main(int argc, char *argv[])
{
  LogComponentEnable("UdpReceiver", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServerClientApplication", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(5); // One sender, four receivers

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("OfdmRate6Mbps"),
                               "ControlMode", StringValue("OfdmRate6Mbps"));

  YansWifiPhyHelper phy;
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  mac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Enable broadcast
  for (uint32_t i = 0; i < nodes.GetN(); ++i)
  {
    Ptr<Ipv4L3Protocol> ipv4 = nodes.Get(i)->GetObject<Ipv4L3Protocol>();
    if (ipv4)
    {
      ipv4->SetAttribute("IpForward", BooleanValue(false));
      ipv4->SetAttribute("WeakEsBcast", BooleanValue(true));
    }
  }

  // Install UDP receiver applications on all nodes except the first one
  for (uint32_t i = 1; i < nodes.GetN(); ++i)
  {
    UdpReceiverHelper receiverHelper;
    ApplicationContainer apps = receiverHelper.Install(nodes.Get(i));
    apps.Start(Seconds(0.0));
    apps.Stop(Seconds(10.0));
  }

  // Configure sender application
  uint16_t port = 9;
  UdpClientHelper client(interfaces.GetAddress(0), port);
  client.SetAttribute("MaxPackets", UintegerValue(10));
  client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = client.Install(nodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}