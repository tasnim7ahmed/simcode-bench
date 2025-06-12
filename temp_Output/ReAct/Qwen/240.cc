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

class UdpServerWithLogging : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    static TypeId tid = TypeId("UdpServerWithLogging")
                            .SetParent<Application>()
                            .SetGroupName("Applications")
                            .AddConstructor<UdpServerWithLogging>();
    return tid;
  }

  UdpServerWithLogging()
      : m_socket(0), m_totalRx(0)
  {
  }

  virtual ~UdpServerWithLogging()
  {
  }

protected:
  void DoDispose() override
  {
    m_socket = 0;
    Application::DoDispose();
  }

private:
  void StartApplication() override
  {
    if (!m_socket)
    {
      TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket(GetNode(), tid);
      InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
      m_socket->Bind(local);
      m_socket->SetRecvCallback(MakeCallback(&UdpServerWithLogging::HandleRead, this));
    }
  }

  void StopApplication() override
  {
    if (m_socket)
    {
      m_socket->Close();
    }
  }

  void HandleRead(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      m_totalRx += packet->GetSize();
      NS_LOG_INFO("Received packet of size " << packet->GetSize() << " at " << Simulator::Now().GetSeconds()
                                            << "s from " << from);
    }
  }

  Ptr<Socket> m_socket;
  uint64_t m_totalRx;
};

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  NodeContainer nodes;
  nodes.Create(4); // One sender, three receivers

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);
  wifi.SetRemoteStationManager("ns3::ArfWifiManager");

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default();

  NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(4),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  Ipv4Address multicastGroup("225.1.2.4");

  for (uint32_t i = 1; i < nodes.GetN(); ++i)
  {
    Ptr<Node> node = nodes.Get(i);
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    uint32_t interface = 0;
    ipv4->AddMulticastRoute(multicastGroup, interfaces.GetAddress(i), interface);
  }

  TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
  Ptr<Socket> senderSocket = Socket::CreateSocket(nodes.Get(0), tid);
  InetSocketAddress receiverAddress(multicastGroup, 9);
  senderSocket->SetAllowBroadcast(true);
  senderSocket->Connect(receiverAddress);

  OnOffHelper onoff("ns3::UdpSocketFactory", Address());
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate("100kbps")));
  onoff.SetAttribute("PacketSize", UintegerValue(512));

  ApplicationContainer apps;
  for (uint32_t i = 1; i < nodes.GetN(); ++i)
  {
    Ptr<Node> node = nodes.Get(i);
    Ptr<Application> app = CreateObject<UdpServerWithLogging>();
    node->AddApplication(app);
    app->SetStartTime(Seconds(1.0));
    app->SetStopTime(Seconds(10.0));
  }

  onoff.SetAttribute("Remote", AddressValue(receiverAddress));
  apps.Add(onoff.Install(nodes.Get(0)));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}