#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/olsr-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("AdhocGridSimulation");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = false;
  bool pcapTracing = false;
  bool asciiTracing = false;
  std::string phyMode ("DsssRate1Mbps");
  uint32_t packetSize = 1000;
  uint32_t numPackets = 1;
  double interval = 1.0;
  double simulationTime = 10.0;
  double gridWidth = 5.0;
  uint32_t gridSize = 5;
  uint32_t sourceNode = 24;
  uint32_t sinkNode = 0;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log ifconfig interfaces.", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing.", tracing);
  cmd.AddValue ("pcapTracing", "Enable pcap tracing.", pcapTracing);
  cmd.AddValue ("asciiTracing", "Enable ascii tracing.", asciiTracing);
  cmd.AddValue ("phyMode", "Wifi phy mode", phyMode);
  cmd.AddValue ("packetSize", "Size of packet in bytes", packetSize);
  cmd.AddValue ("numPackets", "Number of packets generated", numPackets);
  cmd.AddValue ("interval", "Interval between packets in seconds", interval);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("gridWidth", "Grid width in meters", gridWidth);
  cmd.AddValue ("gridSize", "Grid size (number of nodes per side)", gridSize);
  cmd.AddValue ("sourceNode", "Source node number", sourceNode);
  cmd.AddValue ("sinkNode", "Sink node number", sinkNode);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (gridSize * gridSize);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiChannel.SetPropagationDelay ("Model", "ConstantSpeedPropagationDelayModel");

  Ptr<RangePropagationLossModel> rangeLossModel = CreateObject<RangePropagationLossModel> ();
  rangeLossModel->SetMaxRange (50);
  wifiChannel.AddPropagationLoss ("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(50.0));

  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");
  Ssid ssid = Ssid ("adhoc-grid");
  wifiMac.SetParam ("Ssid", SsidValue (ssid));

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  OlsrHelper olsr;
  InternetStackHelper internet;
  internet.SetRoutingHelper (olsr);
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (gridWidth),
                                 "DeltaY", DoubleValue (gridWidth),
                                 "GridWidth", UintegerValue (gridSize),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (interfaces.GetAddress (sinkNode), 9));
  ApplicationContainer sinkApps = sink.Install (nodes.Get (sinkNode));
  sinkApps.Start (Seconds (1.0));
  sinkApps.Stop (Seconds (simulationTime));

  Ptr<Node> source = nodes.Get (sourceNode);
  Ptr<UdpClient> client = CreateObject<UdpClient> ();
  client->SetAttribute ("RemoteAddress", AddressValue (InetSocketAddress (interfaces.GetAddress (sinkNode), 9)));
  client->SetAttribute ("RemotePort", UintegerValue (9));
  client->SetAttribute ("PacketSize", UintegerValue (packetSize));
  client->SetAttribute ("MaxPackets", UintegerValue (numPackets));
  client->SetAttribute ("Interval", TimeValue (Seconds (interval)));
  source->AddApplication (client);
  client->SetStartTime (Seconds (2.0));
  client->SetStopTime (Seconds (simulationTime));

  if (verbose)
    {
      for (uint32_t i = 0; i < nodes.GetN (); ++i)
        {
          Ptr<Ipv4> ipv4 = nodes.Get (i)->GetObject<Ipv4> ();
          Ipv4InterfaceAddress iaddr = ipv4->GetAddress (1, 0);
          std::cout << "Node " << i << " has address " << iaddr.GetLocal () << std::endl;
        }
    }

  if (tracing)
    {
      PointToPointHelper pointToPoint;
      pointToPoint.EnablePcapAll ("adhoc-grid");
    }

  if (pcapTracing)
    {
      wifiPhy.EnablePcapAll ("adhoc-grid");
    }

  if (asciiTracing)
    {
      AsciiTraceHelper ascii;
      wifiPhy.EnableAsciiAll (ascii.CreateFileStream ("adhoc-grid.tr"));
    }
  
  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}