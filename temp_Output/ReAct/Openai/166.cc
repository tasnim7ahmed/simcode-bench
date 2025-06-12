#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocAodvUdpExample");

void
RxTraceCallback(std::string context, Ptr<const Packet> packet, const Address &address)
{
  NS_LOG_UNCOND ("Packet received at " << Simulator::Now ().GetSeconds () << "s, size: " << packet->GetSize ());
}

void
TxTraceCallback(std::string context, Ptr<const Packet> packet)
{
  NS_LOG_UNCOND ("Packet transmitted at " << Simulator::Now ().GetSeconds () << "s, size: " << packet->GetSize ());
}

void
RouteRequestTrace(std::string context, const Ipv4Header &header)
{
  NS_LOG_UNCOND ("AODV RREQ sent at " << Simulator::Now ().GetSeconds () << "s, destination: " << header.GetDestination ());
}

void
RouteReplyTrace(std::string context, const Ipv4Header &header)
{
  NS_LOG_UNCOND ("AODV RREP sent at " << Simulator::Now ().GetSeconds () << "s, destination: " << header.GetDestination ());
}

int
main (int argc, char *argv[])
{
  LogComponentEnable ("AdhocAodvUdpExample", LOG_LEVEL_INFO);

  uint32_t numNodes = 5;
  double simTime = 20.0;

  NodeContainer nodes;
  nodes.Create (numNodes);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.Set ("TxPowerStart", DoubleValue (16.0));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (16.0));

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  InternetStackHelper stack;
  AodvHelper aodv;
  stack.SetRoutingHelper (aodv);
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Enable packet tracing
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyTxBegin", MakeCallback (&TxTraceCallback));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxEnd", MakeCallback (&RxTraceCallback));
  Config::Connect ("/NodeList/*/$ns3::aodv::RoutingProtocol/RouteRequest", MakeCallback (&RouteRequestTrace));
  Config::Connect ("/NodeList/*/$ns3::aodv::RoutingProtocol/RouteReply", MakeCallback (&RouteReplyTrace));

  // UDP Server Setup (Node 4)
  uint16_t serverPort = 4000;
  UdpServerHelper serverHelper (serverPort);
  ApplicationContainer serverApp = serverHelper.Install (nodes.Get (4)); // server at node 4
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (simTime));

  // UDP Client Setup (Node 0)
  UdpClientHelper client (interfaces.GetAddress (4), serverPort);
  client.SetAttribute ("MaxPackets", UintegerValue (1000));
  client.SetAttribute ("Interval", TimeValue (Seconds (0.2)));
  client.SetAttribute ("PacketSize", UintegerValue (64));
  ApplicationContainer clientApp = client.Install (nodes.Get (0)); // client at node 0
  clientApp.Start (Seconds (2.0));
  clientApp.Stop (Seconds (simTime));

  // Optional: Enable flow monitor
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();
  monitor->SerializeToXmlFile ("aodv-adhoc-flowmon.xml", true, true);
  Simulator::Destroy ();
  return 0;
}