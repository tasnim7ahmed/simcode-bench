#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/vanet-mobility-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VanetDsdvSimulation");

int main(int argc, char* argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);
  LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

  // Create Nodes
  NodeContainer nodes;
  nodes.Create(5); // Number of vehicles
  NodeContainer rsu;
  rsu.Create(1); // Roadside unit

  // Install Internet Stack
  InternetStackHelper internet;
  internet.Install(nodes);
  internet.Install(rsu);

  DsdvHelper dsdv;
  dsdv.Set("PeriodicUpdateInterval", TimeValue(Seconds(5))); //Example parameter setting
  dsdv.Install(nodes);
  dsdv.Install(rsu);

  // Assign Addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(nodes);
  Ipv4InterfaceContainer rsuInterfaces = ipv4.Assign(rsu);

  // Mobility Model
  WaypointMobilityModelHelper mobility;
  mobility.SetSpeed(10); // m/s
  mobility.SetWaitTime(Seconds(0));

  // Define waypoints for each node (simple straight line)
  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
    Ptr<Node> node = nodes.Get(i);
    Ptr<WaypointMobilityModel> wpmm = CreateObject<WaypointMobilityModel>();
    node->AggregateObject(wpmm);

    wpmm->AddWaypoint(Waypoint{Seconds(0), Vector3D(i * 10, 0, 0)}); // Start position
    wpmm->AddWaypoint(Waypoint{Seconds(30), Vector3D(i * 10 + 300, 0, 0)}); // End position after 30 seconds

  }

  // Mobility for RSU (static)
  MobilityHelper rsuMobility;
  rsuMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  rsuMobility.Install(rsu);

  // UDP Client-Server Application
  UdpClientServerHelper echoClient(rsuInterfaces.GetAddress(0), 9); // RSU IP address, port 9
  echoClient.SetAttribute("MaxPackets", UintegerValue(100));
  echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
  echoClient.SetAttribute("PacketSize", UintegerValue(1024));

  ApplicationContainer clientApps = echoClient.Install(nodes);
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(30.0));

  UdpClientServerHelper echoServer(9);
  ApplicationContainer serverApps = echoServer.Install(rsu);
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(30.0));

  // Run Simulation
  Simulator::Stop(Seconds(30.0));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}