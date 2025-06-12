#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTcpSimulation");

class TcpServerClientSimulation {
public:
  TcpServerClientSimulation();
  void Run();

private:
  void SetupNetwork();
  void SetupApplications();
  NodeContainer m_nodes;
  NetDeviceContainer m_devices;
  Ipv4InterfaceContainer m_interfaces;
};

TcpServerClientSimulation::TcpServerSimulation()
{
  // Nothing here yet
}

void
TcpServerClientSimulation::Run()
{
  SetupNetwork();
  SetupApplications();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();
}

void
TcpServerClientSimulation::SetupNetwork()
{
  m_nodes.Create(2);

  WifiMacHelper wifiMac;
  WifiHelper wifiHelper;
  wifiHelper.SetStandard(WIFI_STANDARD_80211b);
  wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("DsssRate11Mbps"), "ControlMode", StringValue("DsssRate11Mbps"));

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  m_devices = wifiHelper.Install(phy, wifiMac, m_nodes);

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(m_nodes);

  InternetStackHelper stack;
  stack.Install(m_nodes);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  m_interfaces = address.Assign(m_devices);
}

void
TcpServerClientSimulation::SetupApplications()
{
  // Server application (TCP Echo)
  uint16_t port = 9;
  PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer serverApps = sink.Install(m_nodes.Get(1));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  // Client application
  OnOffHelper client("ns3::TcpSocketFactory", InetSocketAddress(m_interfaces.GetAddress(1), port));
  client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  client.SetAttribute("DataRate", DataRateValue(DataRate("1kbps")));
  client.SetAttribute("PacketSize", UintegerValue(1024));
  client.SetAttribute("MaxBytes", UintegerValue(0)); // Infinite

  ApplicationContainer clientApp = client.Install(m_nodes.Get(0));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  // Set TCP variant to Reno
  Config::Set("/NodeList/*/$$ns3::TcpL4Protocol$$/SocketType", TypeIdValue(TcpReno::GetTypeId()));
}

int
main(int argc, char *argv[])
{
  TcpServerClientSimulation simulation;
  simulation.Run();
  return 0;
}