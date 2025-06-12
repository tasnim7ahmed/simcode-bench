#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiTwoAPsMultipleStations");

class StationTrafficCounter : public Application
{
public:
  static TypeId GetTypeId(void)
  {
    TypeId tid = TypeId("StationTrafficCounter")
      .SetParent<Application>()
      .SetGroupName("Tutorial")
      .AddConstructor<StationTrafficCounter>();
    return tid;
  }

  StationTrafficCounter() {}
  virtual ~StationTrafficCounter() {}

  void SetRemote(Address address)
  {
    m_peer = address;
  }

protected:
  void DoInitialize() override
  {
    Application::DoInitialize();
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->Connect(m_peer);
  }

  void StartApplication() override
  {
    Simulator::ScheduleNow(&StationTrafficCounter::SendPacket, this);
  }

  void StopApplication() override
  {
    if (m_socket)
    {
      m_socket->Close();
    }
  }

private:
  void SendPacket()
  {
    Ptr<Packet> packet = Create<Packet>(1024); // 1KB payload
    m_socket->Send(packet);

    m_totalTx += 1024;
    NS_LOG_INFO("Sent packet of size 1024 bytes. Total TX: " << m_totalTx << " bytes");

    Simulator::Schedule(MilliSeconds(100), &StationTrafficCounter::SendPacket, this);
  }

  Address m_peer;
  Ptr<Socket> m_socket;
  uint64_t m_totalTx = 0;
};

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  LogComponentEnable("WiFiTwoAPsMultipleStations", LOG_LEVEL_INFO);

  NodeContainer apNodes;
  apNodes.Create(2);

  NodeContainer staNodesGroup1, staNodesGroup2;
  staNodesGroup1.Create(3);
  staNodesGroup2.Create(3);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"));

  // AP MAC configuration
  mac.SetType("ns3::ApWifiMac");
  NetDeviceContainer apDevices1 = wifi.Install(phy, mac, apNodes.Get(0));
  NetDeviceContainer apDevices2 = wifi.Install(phy, mac, apNodes.Get(1));

  // STA MAC configuration
  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(), "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevicesGroup1 = wifi.Install(phy, mac, staNodesGroup1);
  NetDeviceContainer staDevicesGroup2 = wifi.Install(phy, mac, staNodesGroup2);

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(5.0),
                                "GridWidth", UintegerValue(5),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNodes);
  mobility.Install(staNodesGroup1);
  mobility.Install(staNodesGroup2);

  // Internet stack
  InternetStackHelper stack;
  stack.InstallAll();

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");

  Ipv4InterfaceContainer apInterfaces1, apInterfaces2;
  Ipv4InterfaceContainer staInterfacesGroup1, staInterfacesGroup2;

  apInterfaces1 = address.Assign(apDevices1);
  address.NewNetwork();
  apInterfaces2 = address.Assign(apDevices2);

  address.NewNetwork();
  staInterfacesGroup1 = address.Assign(staDevicesGroup1);
  address.NewNetwork();
  staInterfacesGroup2 = address.Assign(staDevicesGroup2);

  // Server setup on AP1 and AP2
  UdpServerHelper serverHelper(4000);
  ApplicationContainer serverApp1 = serverHelper.Install(apNodes.Get(0));
  serverApp1.Start(Seconds(1.0));
  serverApp1.Stop(Seconds(10.0));

  ApplicationContainer serverApp2 = serverHelper.Install(apNodes.Get(1));
  serverApp2.Start(Seconds(1.0));
  serverApp2.Stop(Seconds(10.0));

  // Client setup for stations
  for (uint32_t i = 0; i < staNodesGroup1.GetN(); ++i)
  {
    Ptr<Node> sta = staNodesGroup1.Get(i);
    Ptr<Ipv4> ipv4 = sta->GetObject<Ipv4>();
    Ipv4InterfaceAddress iaddr = ipv4->GetInterfaceAddress(1);
    Address serverAddress(InetSocketAddress(apInterfaces1.GetAddress(0), 4000));

    ApplicationContainer app;
    StationTrafficCounterHelper clientHelper;
    clientHelper.SetAttribute("Remote", AddressValue(serverAddress));
    app.Add(clientHelper.Install(sta));
    app.Start(Seconds(2.0));
    app.Stop(Seconds(10.0));
  }

  for (uint32_t i = 0; i < staNodesGroup2.GetN(); ++i)
  {
    Ptr<Node> sta = staNodesGroup2.Get(i);
    Ptr<Ipv4> ipv4 = sta->GetObject<Ipv4>();
    Ipv4InterfaceAddress iaddr = ipv4->GetInterfaceAddress(1);
    Address serverAddress(InetSocketAddress(apInterfaces2.GetAddress(0), 4000));

    ApplicationContainer app;
    StationTrafficCounterHelper clientHelper;
    clientHelper.SetAttribute("Remote", AddressValue(serverAddress));
    app.Add(clientHelper.Install(sta));
    app.Start(Seconds(2.0));
    app.Stop(Seconds(10.0));
  }

  // Tracing
  phy.EnablePcapAll("wifi_two_aps_multiple_stas");
  AsciiTraceHelper ascii;
  phy.EnableAsciiAll(ascii.CreateFileStream("wifi_two_aps_multiple_stas.tr"));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}