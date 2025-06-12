#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/wifi-helper.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/aodv-module.h"

using namespace ns3;

uint64_t bytesReceived = 0;

void
RxCallback (Ptr<const Packet> packet, const Address &address)
{
  bytesReceived += packet->GetSize ();
}

int
main (int argc, char *argv[])
{
  double simulationTime = 10.0; // seconds
  uint16_t port = 50000;
  std::string dataRate = "5Mbps";
  std::string packetSize = "1024";
  uint32_t maxBytes = 0; // 0 means unlimited

  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Wifi channel and physical layer
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  // Wifi MAC and helper
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211g);
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");
  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-wifi-tcp");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, nodes.Get (1));

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install (phy, mac, nodes.Get (0));

  NetDeviceContainer devices;
  devices.Add (apDevice.Get (0));
  devices.Add (staDevices.Get (0));

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (5.0, 0.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // AODV Routing + TCP/IP Stack
  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingHelper (aodv);
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // TCP Server: BulkSendApplication on node 0 (server receives data)
  uint32_t serverNode = 0;
  uint32_t clientNode = 1;

  // Setup PacketSink (server)
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory",
                              InetSocketAddress (interfaces.GetAddress (serverNode), port));
  ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get (serverNode));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simulationTime + 1));

  // Setup BulkSendApplication (client)
  BulkSendHelper source ("ns3::TcpSocketFactory",
                        InetSocketAddress (interfaces.GetAddress (serverNode), port));
  source.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
  source.SetAttribute ("SendSize", UintegerValue (std::stoi(packetSize)));
  ApplicationContainer sourceApp = source.Install (nodes.Get (clientNode));
  sourceApp.Start (Seconds (1.0));
  sourceApp.Stop (Seconds (simulationTime));

  // Enable packet reception callback for throughput calculation
  Ptr<PacketSink> sink = DynamicCast<PacketSink> (sinkApp.Get (0));
  sink->TraceConnectWithoutContext ("Rx", MakeCallback (&RxCallback));

  // Enable NetAnim
  AnimationInterface anim ("wifi-tcp-netanim.xml");
  anim.SetConstantPosition (nodes.Get (0), 0, 0);
  anim.SetConstantPosition (nodes.Get (1), 5, 0);

  Simulator::Stop (Seconds (simulationTime + 1.5));
  Simulator::Run ();

  double throughput = (bytesReceived * 8.0) / (simulationTime * 1e6);
  std::cout << "Received bytes: " << bytesReceived << std::endl;
  std::cout << "Throughput: " << throughput << " Mbps" << std::endl;

  Simulator::Destroy ();
  return 0;
}