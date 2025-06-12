#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MulticastWifiSimulation");

class MulticastReceiver : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("MulticastReceiver")
      .SetParent<Application>()
      .AddConstructor<MulticastReceiver>();
    return tid;
  }

  MulticastReceiver() {}
  virtual ~MulticastReceiver() {}

private:
  virtual void StartApplication()
  {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
    m_socket->Bind(local);
    m_socket->SetRecvCallback(MakeCallback(&MulticastReceiver::ReceivePacket, this));
    m_socket->SetAllowBroadcast(true);
    m_socket->JoinGroup(Ipv4Address("225.1.2.4"));
  }

  virtual void StopApplication()
  {
    if (m_socket)
    {
      m_socket->Close();
    }
  }

  void ReceivePacket(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      NS_LOG_UNCOND("Received packet of size " << packet->GetSize() << " at node " << GetNode()->GetId());
    }
  }

  Ptr<Socket> m_socket;
};

int main(int argc, char *argv[])
{
  LogComponentEnable("MulticastReceiver", LOG_LEVEL_INFO);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(3);

  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);

  mac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

  mac.SetType("ns3::ApWifiMac");
  NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(5.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiStaNodes);
  mobility.Install(wifiApNode);

  InternetStackHelper stack;
  stack.InstallAll();

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer apInterface;
  Ipv4InterfaceContainer staInterfaces;
  apInterface = address.Assign(apDevice);
  staInterfaces = address.Assign(staDevices);

  Ipv4Address multicastSource("10.1.1.1");
  Ipv4Address multicastGroup("225.1.2.4");

  Ipv4StaticRoutingHelper multicast;
  for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
  {
    Ptr<Node> node = wifiStaNodes.Get(i);
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    uint32_t netdev_idx = ipv4->GetInterfaceForAddress(staInterfaces.GetAddress(i));
    multicast.AddMulticastRoute(node, multicastSource, multicastGroup, netdev_idx);
  }

  {
    Ptr<Node> node = wifiApNode.Get(0);
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    uint32_t netdev_idx = ipv4->GetInterfaceForAddress(apInterface.GetAddress(0));
    multicast.AddMulticastRoute(node, multicastSource, multicastGroup, netdev_idx);
  }

  ApplicationContainer receivers;
  for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
  {
    Ptr<MulticastReceiver> app = CreateObject<MulticastReceiver>();
    wifiStaNodes.Get(i)->AddApplication(app);
    app->SetStartTime(Seconds(1.0));
    app->SetStopTime(Seconds(10.0));
  }

  uint16_t port = 9;
  OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(multicastGroup, port));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate("100kbps")));
  onoff.SetAttribute("PacketSize", UintegerValue(512));

  ApplicationContainer senderApp;
  Ptr<OnOffApplication> sender = DynamicCast<OnOffApplication>(onoff.Install(wifiApNode.Get(0)).Get(0));
  senderApp.Add(sender);
  sender->SetStartTime(Seconds(2.0));
  sender->SetStopTime(Seconds(9.0));

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}