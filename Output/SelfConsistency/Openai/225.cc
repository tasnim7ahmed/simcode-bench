#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VanetDsdvExample");

void ReceivePacket(Ptr<Socket> socket)
{
  while (Ptr<Packet> packet = socket->Recv ())
    {
      // Just to acknowledge receipt
      // NS_LOG_INFO("Packet received at RSU: " << packet->GetSize());
    }
}

int main(int argc, char *argv[])
{
  uint32_t numVehicles = 10;
  double simTime = 30.0;
  double highwayLength = 500.0;
  double laneYOffset = 5.0;
  double vehicleSpeed = 25.0; // m/s ~ 90 km/h
  double packetInterval = 1.0; // seconds
  uint16_t udpPort = 4000;

  CommandLine cmd;
  cmd.AddValue("numVehicles", "Number of vehicles", numVehicles);
  cmd.Parse(argc, argv);

  // Nodes: [0, N-1] - vehicles, [N] - RSU
  NodeContainer allNodes;
  allNodes.Create(numVehicles + 1);
  NodeContainer vehicleNodes;
  vehicleNodes.Add(allNodes.Get(0), allNodes.Get(numVehicles - 1));
  for(uint32_t i = 0; i < numVehicles; ++i)
      vehicleNodes.Add(allNodes.Get(i));
  Ptr<Node> rsuNode = allNodes.Get(numVehicles);

  // Wifi configuration
  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211p);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  NqosWaveMacHelper wifiMac = NqosWaveMacHelper::Default();
  NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, allNodes);

  // Mobility setup
  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::WaypointMobilityModel");
  mobility.Install(allNodes);

  // Set waypoints for vehicles
  for(uint32_t i = 0; i < numVehicles; ++i)
  {
    Ptr<WaypointMobilityModel> mob = allNodes.Get(i)->GetObject<WaypointMobilityModel>();
    Vector start(i * (highwayLength / numVehicles), laneYOffset, 0);
    Vector end(highwayLength, laneYOffset, 0);
    double travelTime = (highwayLength - start.x) / vehicleSpeed;
    mob->AddWaypoint(Waypoint(Seconds(0.0), start));
    mob->AddWaypoint(Waypoint(Seconds(travelTime), end));
    mob->AddWaypoint(Waypoint(Seconds(simTime), end));
  }

  // RSU: stationary, at end of highway
  Ptr<WaypointMobilityModel> rsuMob = allNodes.Get(numVehicles)->GetObject<WaypointMobilityModel>();
  Vector rsuPos(highwayLength + 10, laneYOffset, 0);
  rsuMob->AddWaypoint(Waypoint(Seconds(0.0), rsuPos));
  rsuMob->AddWaypoint(Waypoint(Seconds(simTime), rsuPos));

  // Internet stack with DSDV
  InternetStackHelper stack;
  DsdvHelper dsdv;
  stack.SetRoutingHelper(dsdv);
  stack.Install(allNodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);
  Ipv4Address rsuAddress = interfaces.GetAddress(numVehicles);

  // Set up RSU UDP sink
  TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
  Ptr<Socket> rsuSink = Socket::CreateSocket(rsuNode, tid);
  InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), udpPort);
  rsuSink->Bind(local);
  rsuSink->SetRecvCallback(MakeCallback(&ReceivePacket));

  // Applications: Each vehicle