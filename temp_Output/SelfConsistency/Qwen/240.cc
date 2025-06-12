#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MulticastWifiSimulation");

class MulticastReceiver : public Application
{
public:
  MulticastReceiver();
  virtual ~MulticastReceiver();

  static TypeId GetTypeId(void);
  void DoInitialize(void);

protected:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

private:
  void ReceivePacket(Ptr<Socket> socket);

  Ptr<Socket> m_socket;
};

MulticastReceiver::MulticastReceiver()
    : m_socket(nullptr)
{
}

MulticastReceiver::~MulticastReceiver()
{
  m_socket = nullptr;
}

TypeId MulticastReceiver::GetTypeId(void)
{
  static TypeId tid = TypeId("MulticastReceiver")
                          .SetParent<Application>()
                          .SetGroupName("Tutorial")
                          .AddConstructor<MulticastReceiver>();
  return tid;
}

void MulticastReceiver::DoInitialize(void)
{
  Application::DoInitialize();
}

void MulticastReceiver::StartApplication(void)
{
  m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
  m_socket->Bind(local);
  m_socket->JoinGroup(Ipv4Address("225.1.2.4"));
  m_socket->SetRecvCallback(MakeCallback(&MulticastReceiver::ReceivePacket, this));
}

void MulticastReceiver::StopApplication(void)
{
  if (m_socket)
    {
      m_socket->Close();
      m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    }
}

void MulticastReceiver::ReceivePacket(Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from)))
    {
      NS_LOG_INFO("Received packet of size " << packet->GetSize() << " from " << from);
    }
}

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  LogComponentEnable("MulticastWifiSimulation", LOG_LEVEL_INFO);
  LogComponentEnable("MulticastReceiver", LOG_LEVEL_INFO);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(3); // One sender and two receivers

  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  WifiHelper wifi;

  NetDeviceContainer staDevices;
  mac.SetType("ns3::StaWifiMac");
  staDevices = wifi.Install(phy, mac, wifiStaNodes);

  NetDeviceContainer apDevice;
  mac.SetType("ns3::ApWifiMac");
  apDevice = wifi.Install(phy, mac, wifiApNode);

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

  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign(staDevices);
  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign(apDevice);

  // Set up multicast
  Ipv4StaticRoutingHelper multicast;
  Ptr<Node> apNode = wifiApNode.Get(0);
  Ptr<Ipv4> ipv4 = apNode->GetObject<Ipv4>();
  uint32_t netDeviceIndex = ipv4->GetInterfaceForAddress(apInterface.GetAddress(0));
  ipv4->SetDownstreamDefaultMulticastRoute(netDeviceIndex);

  for (auto i = 0; i < 2; ++i)
    {
      Ptr<Node> node = wifiStaNodes.Get(i + 1); // Receivers
      ObjectFactory receiverFactory;
      receiverFactory.SetTypeId("MulticastReceiver");
      Ptr<Application> app = receiverFactory.Create<MulticastReceiver>();
      node->AddApplication(app);
      app->SetStartTime(Seconds(1.0));
      app->SetStopTime(Seconds(10.0));
    }

  uint16_t port = 9;
  Address remoteAddress = InetSocketAddress(Ipv4Address("225.1.2.4"), port);

  OnOffHelper onoff("ns3::UdpSocketFactory", remoteAddress);
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate("1kbps")));
  onoff.SetAttribute("PacketSize", UintegerValue(512));

  ApplicationContainer apps;
  apps.Add(onoff.Install(wifiStaNodes.Get(0))); // Sender is first STA
  apps.Start(Seconds(2.0));
  apps.Stop(Seconds(10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}