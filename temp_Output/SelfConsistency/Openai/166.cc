#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/packet.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AodvAdhocSimulation");

void
RxTrace (Ptr<const Packet> packet, const Address &address)
{
  NS_LOG_UNCOND ("Packet received at " << Simulator::Now ().GetSeconds () << " s");
}

int 
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);
  LogComponentEnable ("AodvAdhocSimulation", LOG_LEVEL_INFO);

  uint32_t numNodes = 5;
  double simTime = 20.0;

  NodeContainer nodes;
  nodes.Create (numNodes);

  // Set up Wifi
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.Set ("RxGain", DoubleValue (0)); 
  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::RangePropagationLossModel", "MaxRange", DoubleValue (100.0));
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac"); 

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // Install internet stack with AODV
  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingHelper (aodv);
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // UDP server on node 0
  uint16_t port = 9999;
  UdpServerHelper server (port);
  ApplicationContainer serverApp = server.Install (nodes.Get (0));
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (simTime));

  // UDP client on node 4, sending to node 0
  UdpClientHelper client (interfaces.GetAddress (0), port);
  client.SetAttribute ("MaxPackets", UintegerValue (1000));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.5)));
  client.SetAttribute ("PacketSize", UintegerValue (512));
  ApplicationContainer clientApp = client.Install (nodes.Get (4));
  clientApp.Start (Seconds (2.0));
  clientApp.Stop (Seconds (simTime));

  // Packet tracing at receiver
  Ptr<Node> serverNode = nodes.Get (0);
  Ptr<Ipv4L3Protocol> ipv4 = serverNode->GetObject<Ipv4L3Protocol> ();
  ipv4->TraceConnectWithoutContext ("Receive", MakeCallback (&RxTrace));

  // Enable PCAP tracing
  wifiPhy.EnablePcapAll ("aodv-adhoc");

  // FlowMonitor for statistics
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  monitor->SerializeToXmlFile ("aodv-adhoc-flowmon.xml", true, true);

  Simulator::Destroy ();
  return 0;
}