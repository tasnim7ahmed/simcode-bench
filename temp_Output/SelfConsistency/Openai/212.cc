#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AodvAdhocExample");

int
main (int argc, char *argv[])
{
  // Enable logging for AODV
  LogComponentEnable ("AodvRoutingProtocol", LOG_LEVEL_INFO);

  // Simulation parameters
  uint32_t numNodes = 5;
  double simTime = 20.0; // seconds
  double areaSize = 100.0;

  CommandLine cmd;
  cmd.AddValue ("simTime", "Duration of the simulation in seconds", simTime);
  cmd.Parse (argc, argv);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (numNodes);

  // Set up WiFi physical and MAC layers using adhoc MAC
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices;
  devices = wifi.Install (wifiPhy, wifiMac, nodes);

  // Set mobility: random positions in 100x100m area
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // Set up internet stack with AODV
  AodvHelper aodv;
  InternetStackHelper internet;
  internet.SetRoutingHelper (aodv);
  internet.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // Set up UDP application
  // Node 0 will send UDP packets to Node 4

  // UDP server on node 4
  uint16_t udpPort = 4000;
  UdpServerHelper udpServer (udpPort);
  ApplicationContainer serverApp = udpServer.Install (nodes.Get (4));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (simTime));

  // UDP client on node 0 targeting node 4
  UdpClientHelper udpClient (interfaces.GetAddress (4), udpPort);
  udpClient.SetAttribute ("MaxPackets", UintegerValue (100));
  udpClient.SetAttribute ("Interval", TimeValue (Seconds (0.2)));
  udpClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApp = udpClient.Install (nodes.Get (0));
  clientApp.Start (Seconds (2.0));
  clientApp.Stop (Seconds (simTime));

  // Enable pcap tracing
  wifiPhy.EnablePcapAll ("aodv-adhoc");

  // Animation Interface and ASCII tracing (optional)
  // Uncomment if NetAnim or ascii trace is needed
  //AsciiTraceHelper ascii;
  //wifiPhy.EnableAsciiAll (ascii.CreateFileStream ("aodv-adhoc.tr"));

  // Print routing tables at time 5s, 10s, 15s
  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> (&std::cout);
  aodv.PrintRoutingTableAllAt (Seconds (5.0), routingStream);
  aodv.PrintRoutingTableAllAt (Seconds (10.0), routingStream);
  aodv.PrintRoutingTableAllAt (Seconds (15.0), routingStream);

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}