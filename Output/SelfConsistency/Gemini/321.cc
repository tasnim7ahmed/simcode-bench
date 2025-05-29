#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mesh-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/olsr-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMeshExample");

int main(int argc, char *argv[]) {

  CommandLine cmd;
  cmd.Parse(argc, argv);

  // Set up Wifi with Mesh
  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211s);

  MeshHelper mesh;
  mesh.SetStackInstaller("ns3::Dot11sStack");
  mesh.SetMacType("RandomStart");

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
  wifiPhy.SetChannel(wifiChannel.Create());
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("OfdmRate6Mbps"),
                               "ControlMode", StringValue("OfdmRate6Mbps"));

  // Create Mesh Nodes
  NodeContainer meshNodes;
  meshNodes.Create(4);
  mesh.Install(wifi, wifiPhy, meshNodes);

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(5.0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(meshNodes);

  // Internet Stack
  InternetStackHelper internet;
  internet.Install(meshNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(meshNodes);

  // UDP Server (on the last node)
  UdpServerHelper server(9);
  ApplicationContainer serverApp = server.Install(meshNodes.Get(3));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  // UDP Clients (on the first three nodes)
  UdpClientHelper client(interfaces.GetAddress(3), 9);
  client.SetAttribute("MaxPackets", UintegerValue(1000));
  client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps;
  for (int i = 0; i < 3; ++i) {
    clientApps.Add(client.Install(meshNodes.Get(i)));
  }
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}