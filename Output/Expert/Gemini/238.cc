#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/on-off-application.h"
#include "ns3/udp-client.h"
#include "ns3/udp-server.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/constant-velocity-mobility-model.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211s);

  MeshHelper mesh;
  mesh.SetStackInstaller("ns3::Dot11sStack");
  mesh.SetSpreadInterfaceChannels(MeshHelper::SPREAD_CHANNELS);
  NetDeviceContainer devices = mesh.Install(wifi, nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  Ptr<ConstantPositionMobilityModel> staticMobility = CreateObject<ConstantPositionMobilityModel>();
  nodes.Get(0)->AggregateObject(staticMobility);

  Ptr<ConstantVelocityMobilityModel> mobileMobility = CreateObject<ConstantVelocityMobilityModel>();
  mobileMobility->SetVelocity(Vector(1, 0, 0));
  nodes.Get(1)->AggregateObject(mobileMobility);
  nodes.Get(1)->GetObject<MobilityModel>()->SetPosition(Vector(0, 0, 0));

  uint16_t port = 9;
  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(nodes.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientHelper client(interfaces.GetAddress(0), port);
  client.SetAttribute("MaxPackets", UintegerValue(1000));
  client.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
  client.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApps = client.Install(nodes.Get(1));
  clientApps.Start(Seconds(2.0));
  clientApps.Stop(Seconds(10.0));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}