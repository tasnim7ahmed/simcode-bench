#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/udp-server-client-helper.h"
#include "ns3/ipv4-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MulticastWifiSimulation");

class MulticastServer : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("MulticastServer")
      .SetParent<Application>()
      .AddConstructor<MulticastServer>();
    return tid;
  }

  MulticastServer() {}
  virtual ~MulticastServer() {}

private:
  virtual void StartApplication(void)
  {
    NS_LOG_INFO("Multicast server started.");
  }

  virtual void StopApplication(void)
  {
    NS_LOG_INFO("Multicast server stopped.");
  }
};

void ReceivePacket(Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from)))
  {
    NS_LOG_UNCOND("Received a packet of size: " << packet->GetSize());
  }
}

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(3); // one sender, two receivers

  NodeContainer apNode;
  apNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate54Mbps"));

  mac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

  mac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

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
  mobility.Install(apNode);

  InternetStackHelper stack;
  stack.InstallAll();

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

  Ipv4StaticRoutingHelper ipv4RoutingHelper;

  // Set up multicast group
  Ipv4Address multicastGroup("226.0.0.1");

  // Setup multicast routing on all nodes
  for (uint32_t i = 0; i < NodeContainer::GetNNodesTotal(); ++i)
  {
    Ptr<Node> node = NodeList::GetNode(i);
    Ptr<Ipv4StaticRouting> routing = ipv4RoutingHelper.GetStaticRouting(node->GetObject<Ipv4>());
    routing->AddMulticastRoute(Ipv4Address::GetAny(), multicastGroup, Ipv4InterfaceAddress::GetAny(), 1);
  }

  // Create UDP socket on receivers
  TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
  for (uint32_t i = 1; i < wifiStaNodes.GetN(); ++i) // receivers are index 1 and 2
  {
    Ptr<Socket> recvSink = Socket::CreateSocket(wifiStaNodes.Get(i), tid);
    InetSocketAddress local = InetSocketAddress(multicastGroup, 9);
    recvSink->SetRecvCallback(MakeCallback(&ReceivePacket));
    recvSink->Bind(local);
    recvSink->JoinGroup(multicastGroup);
  }

  // Sender socket
  Ptr<Socket> source = Socket::CreateSocket(wifiStaNodes.Get(0), tid);
  InetSocketAddress remote = InetSocketAddress(multicastGroup, 9);
  source->SetAllowBroadcast(true);

  Simulator::ScheduleWithContext(source->GetNode()->GetId(),
                                 Seconds(1.0), &Socket::SendTo, source,
                                 Create<Packet>(1024), 0, remote);

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}