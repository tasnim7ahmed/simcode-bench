#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ManetAodvSimulation");

void
ReceivePacket (Ptr<const Packet> packet, const Address &address)
{
  // No-op, placeholder for tracing
}

int
main (int argc, char *argv[])
{
  uint32_t numNodes = 5;
  double simTime = 60.0;
  double distance = 50.0;
  double areaWidth = 200.0;
  double areaHeight = 200.0;

  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (numNodes);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.Set ("TxPowerStart", DoubleValue (16.0));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (16.0));
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetMobilityModel (
      "ns3::RandomWaypointMobilityModel",
      "Speed", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
      "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"),
      "PositionAllocator",
      StringValue ("ns3::RandomRectanglePositionAllocator["
                   "X=ns3::UniformRandomVariable[Min=0.0|Max="
                   + std::to_string (areaWidth)
                   + "]|Y=ns3::UniformRandomVariable[Min=0.0|Max="
                   + std::to_string (areaHeight)
                   + "]]"));
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (0.0),
                                "MinY", DoubleValue (0.0),
                                "DeltaX", DoubleValue (distance),
                                "DeltaY", DoubleValue (distance),
                                "GridWidth", UintegerValue (3),
                                "LayoutType", StringValue ("RowFirst"));
  mobility.Install (nodes);

  // Enable mobility trace
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> mobilityStream = ascii.CreateFileStream ("manet-mobility.tr");
  mobility.EnableAsciiAll (mobilityStream);

  AodvHelper aodv;
  InternetStackHelper internet;
  internet.SetRoutingHelper (aodv);
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  uint16_t port = 654;
  double appStartTime = 2.0;
  double appStopTime = simTime - 1;

  // Install UDP echo server on last node
  UdpEchoServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (numNodes - 1));
  serverApps.Start (Seconds (appStartTime));
  serverApps.Stop (Seconds (appStopTime));

  // Install UDP echo client on first node, sending to last node
  UdpEchoClientHelper echoClient (interfaces.GetAddress (numNodes - 1), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (100));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.5)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (64));
  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (appStartTime + 1));
  clientApps.Stop (Seconds (appStopTime));

  // Packet delivery statistics
  uint32_t receivedPackets = 0;
  Config::ConnectWithoutContext ("/NodeList/*/ApplicationList/*/$ns3::UdpEchoServer/Rx",
      MakeCallback ([&] (Ptr<const Packet> packet) {
        receivedPackets++;
      }));

  Simulator::Stop (Seconds (simTime));

  AnimationInterface anim ("manet-aodv.xml");

  Simulator::Run ();

  std::cout << "Packets received by the server: " << receivedPackets << std::endl;

  Simulator::Destroy ();
  return 0;
}