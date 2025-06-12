#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VanetDsdvUdpSimulation");

int main (int argc, char *argv[])
{
  uint32_t numVehicles = 10;
  double distanceBetweenVehicles = 30.0; // meters
  double highwayLength = (numVehicles-1)*distanceBetweenVehicles + 200;
  uint16_t port = 4000;
  double simulationTime = 30.0;

  CommandLine cmd;
  cmd.AddValue ("numVehicles", "Number of vehicle nodes", numVehicles);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (numVehicles + 1); // vehicles + 1 RSU
  NodeContainer vehicles;
  NodeContainer rsu;
  for (uint32_t i = 0; i < numVehicles; ++i)
    {
      vehicles.Add (nodes.Get (i));
    }
  rsu.Add (nodes.Get (numVehicles));

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  WifiMacHelper wifiMac;
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("DsssRate11Mbps"), "ControlMode", StringValue ("DsssRate11Mbps"));
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;

  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  double initialX = 100.0;
  double y = 0.0;

  // Vehicles initial positions and waypoints
  std::vector<Ptr<WaypointMobilityModel>> vehMobilityModels;
  for (uint32_t i = 0; i < numVehicles; ++i)
    {
      double x = initialX + i * distanceBetweenVehicles;
      positionAlloc->Add (Vector (x, y, 0.0));
    }
  // RSU position (centered on the highway)
  double rsuY = 50.0;
  double rsuX = initialX + (numVehicles - 1)/2.0 * distanceBetweenVehicles;
  positionAlloc->Add (Vector (rsuX, rsuY, 0.0));

  mobility.SetPositionAllocator (positionAlloc);

  // Use WaypointMobilityModel for vehicles, ConstantPositionModel for RSU
  mobility.SetMobilityModel ("ns3::WaypointMobilityModel");
  for (uint32_t i = 0; i < numVehicles; ++i)
    {
      mobility.Install (vehicles.Get (i));
    }
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (rsu.Get (0));

  // Add waypoints for vehicles (straight movement along X)
  for (uint32_t i = 0; i < numVehicles; ++i)
    {
      Ptr<Node> node = vehicles.Get (i);
      Ptr<WaypointMobilityModel> wmm = node->GetObject<WaypointMobilityModel> ();
      double startX = initialX + i * distanceBetweenVehicles;
      double speed = 20.0; // m/s
      double endTime = simulationTime;
      double displacement = speed * endTime;
      double endX = startX + displacement;
      // Initial position checked above
      wmm->AddWaypoint (Waypoint (Seconds (0.0), Vector (startX, y, 0.0)));
      wmm->AddWaypoint (Waypoint (Seconds (simulationTime), Vector (endX, y, 0.0)));
    }

  InternetStackHelper internet;
  DsdvHelper dsdv;
  internet.SetRoutingHelper (dsdv);
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // Install UDP server on RSU
  UdpServerHelper udpServer (port);
  ApplicationContainer serverApps = udpServer.Install (rsu.Get (0));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (simulationTime));

  // Install UDP client on each vehicle, periodically send to RSU
  double pktInterval = 1.0; // seconds
  uint32_t pktSize = 512; // bytes
  uint32_t maxPackets = uint32_t (simulationTime/pktInterval);

  for (uint32_t i = 0; i < numVehicles; ++i)
    {
      UdpClientHelper udpClient (interfaces.GetAddress (numVehicles), port);
      udpClient.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
      udpClient.SetAttribute ("Interval", TimeValue (Seconds (pktInterval)));
      udpClient.SetAttribute ("PacketSize", UintegerValue (pktSize));
      ApplicationContainer clientApps = udpClient.Install (vehicles.Get (i));
      clientApps.Start (Seconds (1.0));
      clientApps.Stop (Seconds (simulationTime - 0.1));
    }

  wifiPhy.EnablePcapAll ("vanet-dsdv-udp");

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}