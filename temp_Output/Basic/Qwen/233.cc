#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpBroadcastSimulation");

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

  UdpReceiver() {}
  virtual ~UdpReceiver() {}

private:
  virtual void StartApplication(void)
  {
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
    m_socket->Bind(local);
    m_socket->SetRecvCallback(MakeCallback(&UdpReceiver::Receive, this));
  }

  virtual void StopApplication(void)
  {
    if (m_socket)
      m_socket->Close();
  }

  void Receive(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      NS_LOG_INFO("Received packet of size " << packet->GetSize() << " at time " << Simulator::Now().As(Time::S));
    }
  }

  Ptr<Socket> m_socket;
};

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(5); // One sender, four receivers

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  mac.SetType("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(5),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  // Setup broadcast
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // Install UdpReceiver on all nodes except the first one
  for (uint32_t i = 1; i < nodes.GetN(); ++i)
  {
    UdpReceiverHelper receiverHelper;
    ApplicationContainer receiverApp = receiverHelper.Install(nodes.Get(i));
    receiverApp.Start(Seconds(0.0));
    receiverApp.Stop(Seconds(10.0));
  }

  // Sender application: UdpClient sending to broadcast address
  UdpClientHelper client(interfaces.GetAddress(0), 9);
  client.SetAttribute("MaxPackets", UintegerValue(10));
  client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApp = client.Install(nodes.Get(0));
  clientApp.Start(Seconds(1.0));
  clientApp.Stop(Seconds(10.0));

  // Set destination address to broadcast
  Ptr<UdpClient> udpClient = DynamicCast<UdpClient>(clientApp.Get(0));
  if (udpClient)
  {
    udpClient->SetRemote(Address(interfaces.GetAddress(0).GetSubnetDirectedBroadcast()));
  }

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}