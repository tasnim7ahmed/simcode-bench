#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpWifiAodvExample");

uint64_t totalBytesReceived = 0;

void
ReceivedPacket (Ptr<const Packet> packet, const Address &address)
{
  totalBytesReceived += packet->GetSize();
}

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  LogComponentEnable ("TcpWifiAodvExample", LOG_LEVEL_INFO);

  // Simulation parameters
  double simulationTime = 10.0; // seconds
  uint32_t packetSize = 1024; // bytes
  std::string dataRate = "1Mbps";
  uint32_t numPackets = 10000;

  // Create nodes
  NodeContainer nodes;
  nodes.Create (2);

  // WiFi setup
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.Set ("TxPowerStart", DoubleValue (16.0));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (16.0));

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer wifiDevices = wifi.Install (wifiPhy, wifiMac, nodes);

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(50.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(2),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // Internet stack with AODV
  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingHelper (aodv);
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (wifiDevices);

  // Install TCP server on Node 1
  uint16_t port = 50000;
  Address serverAddress (InetSocketAddress (interfaces.GetAddress(1), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", serverAddress);
  ApplicationContainer serverApp = packetSinkHelper.Install (nodes.Get (1));
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (simulationTime + 1));

  // Install TCP client on Node 0
  OnOffHelper clientHelper ("ns3::TcpSocketFactory", serverAddress);
  clientHelper.SetAttribute ("PacketSize", UintegerValue (packetSize));
  clientHelper.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
  clientHelper.SetAttribute ("MaxBytes", UintegerValue (numPackets * packetSize));
  ApplicationContainer clientApp = clientHelper.Install (nodes.Get (0));
  clientApp.Start (Seconds (1.0));
  clientApp.Stop (Seconds (simulationTime));

  // Connect packet sink trace
  Ptr<PacketSink> sink = DynamicCast<PacketSink> (serverApp.Get (0));
  sink->TraceConnectWithoutContext ("Rx", MakeCallback (&ReceivedPacket));

  // NetAnim
  AnimationInterface anim("tcp-wifi-aodv.xml");
  anim.SetConstantPosition(nodes.Get(0), 0, 50);
  anim.SetConstantPosition(nodes.Get(1), 50, 50);

  // Run simulation
  Simulator::Stop (Seconds (simulationTime + 1));
  Simulator::Run ();

  double throughput = (totalBytesReceived * 8.0) / (simulationTime * 1000000.0); // Mbps

  std::cout << "Total bytes received:   " << totalBytesReceived << std::endl;
  std::cout << "Throughput (Mbps):     " << throughput << std::endl;

  Simulator::Destroy ();
  return 0;
}