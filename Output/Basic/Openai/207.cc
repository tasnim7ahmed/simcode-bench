#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiTcpAodvThroughputExample");

uint64_t totalBytesReceived = 0;

void RxCallback (Ptr<const Packet> packet, const Address &)
{
  totalBytesReceived += packet->GetSize ();
}

int main (int argc, char *argv[])
{
  double simulationTime = 10.0; // seconds
  uint32_t payloadSize = 1024; // bytes per packet
  std::string dataRate = "10Mbps";
  std::string phyMode ("HtMcs7");

  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (2);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211n_2_4GHZ);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-wifi");

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, nodes.Get (0));

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, nodes.Get (1));

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (5.0, 0.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  InternetStackHelper stack;
  AodvHelper aodv;
  stack.SetRoutingHelper (aodv);
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (NetDeviceContainer (staDevices, apDevices));

  uint16_t port = 50000;
  Address apLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", apLocalAddress);
  ApplicationContainer sinkApp = packetSinkHelper.Install (nodes.Get (1));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simulationTime + 1));

  OnOffHelper clientHelper ("ns3::TcpSocketFactory", Address ());
  clientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  clientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  clientHelper.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  clientHelper.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
  AddressValue remoteAddress (InetSocketAddress (interfaces.GetAddress (1), port));
  clientHelper.SetAttribute ("Remote", remoteAddress);

  ApplicationContainer clientApp = clientHelper.Install (nodes.Get (0));
  clientApp.Start (Seconds (1.0));
  clientApp.Stop (Seconds (simulationTime + 1));

  Ptr<Application> sink = sinkApp.Get (0);
  sink->TraceConnectWithoutContext ("Rx", MakeCallback (&RxCallback));

  AnimationInterface anim ("wifi-tcp-aodv.xml");
  anim.SetMobilityPollInterval (Seconds (1));
  anim.UpdateNodeDescription (nodes.Get (0), "TCP Client");
  anim.UpdateNodeDescription (nodes.Get (1), "TCP Server");
  anim.UpdateNodeColor (nodes.Get (0), 0, 255, 0); // green client
  anim.UpdateNodeColor (nodes.Get (1), 0, 0, 255); // blue server

  Simulator::Stop (Seconds (simulationTime + 1));
  Simulator::Run ();

  double throughput = (totalBytesReceived * 8.0) / (simulationTime * 1000000.0); // Mbps
  std::cout << "Throughput: " << throughput << " Mbps" << std::endl;

  Simulator::Destroy ();
  return 0;
}