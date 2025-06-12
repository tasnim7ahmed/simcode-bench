#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mesh-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer meshNodes;
  meshNodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211S);

  MeshHelper mesh;
  mesh.SetStackInstaller(
    "ns3::Dot11sStackHelper",
    "NegotiationProtocol", StringValue("ns3::Dot11sMulticastProtocol"),
    "RootSelectionProtocol", StringValue("ns3::Dot11sRootSelectionProtocol")
  );

  wifi.SetRemoteStationManager("ns3::AarfWifiManager");

  NetDeviceContainer meshDevices = mesh.Install(wifi, meshNodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(meshNodes.Get(1));

  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(meshNodes.Get(0));

  Ptr<ConstantVelocityMobilityModel> mob = meshNodes.Get(0)->GetObject<ConstantVelocityMobilityModel>();
  mob->SetVelocity(Vector3D(1, 0, 0));

  InternetStackHelper internet;
  internet.Install(meshNodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(meshDevices);

  UdpServerHelper server(9);
  ApplicationContainer serverApp = server.Install(meshNodes.Get(1));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  UdpClientHelper client(interfaces.GetAddress(1), 9);
  client.SetAttribute("MaxPackets", UintegerValue(1000));
  client.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
  client.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApp = client.Install(meshNodes.Get(0));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}