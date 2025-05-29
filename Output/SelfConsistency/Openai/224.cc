#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetAodvUdpSimulation");

static void GenerateTraffic (Ptr<Socket> socket, Ipv4InterfaceContainer interfaces, uint32_t nodeId, uint32_t packetSize, uint32_t pktCount, Time pktInterval)
{
  if (pktCount == 0)
    return;

  // Choose a random destination different from self
  Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable> ();
  uint32_t nNodes = interfaces.GetN();
  uint32_t dstNodeId;
  do {
      dstNodeId = uv->GetInteger (0, nNodes - 1);
  } while (dstNodeId == nodeId);

  Ipv4Address dstAddr = interfaces.GetAddress (dstNodeId);

  // Send UDP packet
  socket->Connect (InetSocketAddress (dstAddr, 8000));
  Ptr<Packet> packet = Create<Packet> (packetSize);
  socket->Send (packet);

  // Schedule next packet
  Simulator::Schedule (pktInterval, &GenerateTraffic, socket, interfaces, nodeId, packetSize, pktCount - 1, pktInterval);
}

int main (int argc, char *argv[])
{
  uint32_t nNodes = 10;
  double simTime = 20.0;
  uint32_t packetSize = 512;
  uint32_t numPackets = 20; // per node
  double packetInterval = 1.0; // seconds

  CommandLine cmd;
  cmd.AddValue ("nNodes", "Number of nodes", nNodes);
  cmd.AddValue ("simTime", "Simulation time [s]", simTime);
  cmd.Parse (argc, argv);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (nNodes);

  // Install WiFi
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  // Mobility - RandomWaypoint
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
                             "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"),
                             "PositionAllocator", PointerValue (CreateObjectWithAttributes<RandomRectanglePositionAllocator> (
                                "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=200.0]"),
                                "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=200.0]"))));
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (0.0),
                                "MinY", DoubleValue (0.0),
                                "DeltaX", DoubleValue (20.0),
                                "DeltaY", DoubleValue (20.0),
                                "GridWidth", UintegerValue (5),
                                "LayoutType", StringValue ("RowFirst"));
  mobility.Install (nodes);

  // Internet stack with AODV
  AodvHelper aodv;
  InternetStackHelper internet;
  internet.SetRoutingHelper (aodv);
  internet.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // UDP server on each node
  uint16_t port = 8000;
  ApplicationContainer serverApps;
  for (uint32_t i = 0; i < nNodes; ++i)
    {
      UdpServerHelper udpServer (port);
      ApplicationContainer serverApp = udpServer.Install (nodes.Get(i));
      serverApp.Start (Seconds (0.0));
      serverApp.Stop (Seconds (simTime));
      serverApps.Add (serverApp);
    }

  // UDP client sockets for each node
  for (uint32_t i = 0; i < nNodes; ++i)
    {
      Ptr<Socket> source = Socket::CreateSocket (nodes.Get(i), UdpSocketFactory::GetTypeId ());
      Simulator::ScheduleWithContext (source->GetNode ()->GetId (), Seconds (1.0),
                                      &GenerateTraffic, source, interfaces, i, packetSize, numPackets, Seconds (packetInterval));
    }

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}