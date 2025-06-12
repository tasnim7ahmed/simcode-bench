#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/dsdv-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VANETDsdvSimulation");

class VanetSimulation {
public:
  void Run();
private:
  NodeContainer m_vehicles;
  NodeContainer m_rsu;
  DsdvHelper m_dsdv;
  InternetStackHelper m_internet;
  WifiHelper m_wifi;
  WifiPhyHelper m_wifiPhy;
  WifiMacHelper m_wifiMac;
  MobilityHelper m_mobility;
  PointToPointHelper m_p2p;
  Ipv4AddressHelper m_addressHelper;
  uint16_t m_port = 9;
};

void VanetSimulation::Run() {
  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

  m_vehicles.Create(5);
  m_rsu.Create(1);

  m_wifi.SetStandard(WIFI_STANDARD_80211a);
  m_wifiPhy.Set("ChannelWidth", UintegerValue(20));
  m_wifiMac.SetType("ns3::AdhocWifiMac");

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  m_wifiPhy.SetChannel(wifiChannel.Create());

  NetDeviceContainer devices = m_wifi.Install(m_wifiPhy, m_wifiMac, m_vehicles);
  devices.Add(m_wifi.Install(m_wifiPhy, m_wifiMac, m_rsu));

  m_mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(50.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));

  m_mobility.SetMobilityModel("ns3::WaypointMobilityModel");
  m_mobility.Install(m_vehicles);

  // Add waypoints for movement along highway
  for (auto i = m_vehicles.Begin(); i != m_vehicles.End(); ++i) {
    Ptr<WaypointMobilityModel> wpModel = (*i)->GetObject<WaypointMobilityModel>();
    wpModel->AddWaypoint(Waypoint(Seconds(0.0), Vector(0, 0, 0)));
    wpModel->AddWaypoint(Waypoint(Seconds(30.0), Vector(1000, 0, 0)));
  }

  m_mobility.Install(m_rsu);
  m_rsu.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(500, 10, 0));

  m_internet.SetRoutingHelper(m_dsdv);
  m_internet.Install(m_vehicles);
  m_internet.Install(m_rsu);

  m_addressHelper.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = m_addressHelper.Assign(devices);

  // Connect RSU via point-to-point to the internet
  NodeContainer p2pNodes;
  p2pNodes.Add(m_rsu.Get(0));
  NodeContainer externalNode;
  externalNode.Create(1);
  p2pNodes.Add(externalNode.Get(0));

  m_p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("5Mbps")));
  m_p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

  NetDeviceContainer p2pDevices = m_p2p.Install(p2pNodes);

  m_internet.Install(externalNode);
  interfaces = m_addressHelper.Assign(p2pDevices);

  // Setup UDP echo server on RSU
  UdpEchoServerHelper echoServer(m_port);
  ApplicationContainer serverApps = echoServer.Install(m_rsu.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(30.0));

  // Setup periodic UDP clients in vehicles
  UdpEchoClientHelper echoClient(interfaces.GetAddress(1, 0), m_port);
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(512));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < m_vehicles.GetN(); ++i) {
    clientApps = echoClient.Install(m_vehicles.Get(i));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(30.0));
  }

  Simulator::Stop(Seconds(30.0));
  Simulator::Run();
  Simulator::Destroy();
}

int main(int argc, char *argv[]) {
  VanetSimulation sim;
  sim.Run();
  return 0;
}