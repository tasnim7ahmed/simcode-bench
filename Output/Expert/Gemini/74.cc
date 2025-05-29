#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/aodv-module.h"
#include "ns3/dsr-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ManetRoutingComparison");

int main (int argc, char *argv[])
{
  bool enableFlowMonitor = false;
  std::string routingProtocol = "AODV";
  double simulationTime = 200;
  uint32_t numberOfNodes = 50;
  double maxSpeed = 20;
  double txPowerDbm = 7.5;

  CommandLine cmd;
  cmd.AddValue ("routingProtocol", "Routing Protocol [AODV|DSDV|OLSR|DSR]", routingProtocol);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("numberOfNodes", "Number of nodes", numberOfNodes);
  cmd.AddValue ("maxSpeed", "Maximum speed", maxSpeed);
  cmd.AddValue ("txPower", "Transmit Power (dBm)", txPowerDbm);
  cmd.AddValue ("enableFlowMonitor", "Enable Flow Monitor", enableFlowMonitor);
  cmd.Parse (argc, argv);

  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (numberOfNodes);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());
  phy.Set ("TxPowerStart", DoubleValue (txPowerDbm));
  phy.Set ("TxPowerEnd", DoubleValue (txPowerDbm));

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();
  mac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=1500.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=300.0]"));
  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=" + std::to_string(maxSpeed) + "]"),
                             "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]");
  mobility.Install (nodes);

  AodvHelper aodv;
  OlsrHelper olsr;
  DsdvHelper dsdv;
  DsrHelper dsr;
  DsrMainRoutingHelper dsrRouting;
  Ipv4ListRoutingHelper list;

  if (routingProtocol == "AODV") {
      list.Add (aodv, 10);
  } else if (routingProtocol == "DSDV") {
      list.Add (dsdv, 10);
  } else if (routingProtocol == "OLSR") {
      list.Add (olsr, 10);
  } else if (routingProtocol == "DSR") {
      list.Add (dsrRouting, 10);
  } else {
      std::cerr << "Error: invalid routing protocol" << std::endl;
      return 1;
  }

  InternetStackHelper internet;
  internet.SetRoutingHelper (list);
  internet.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  uint16_t port = 9;

  for (uint32_t i = 0; i < 10; ++i)
    {
      uint32_t src = i % numberOfNodes;
      uint32_t dest = (i + 5) % numberOfNodes;

      UdpServerHelper server (port);
      ApplicationContainer apps = server.Install (nodes.Get (dest));
      apps.Start (Seconds (0.0));
      apps.Stop (Seconds (simulationTime));

      UdpClientHelper client (interfaces.GetAddress (dest), port);
      client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
      client.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
      client.SetAttribute ("PacketSize", UintegerValue (1024));
      apps = client.Install (nodes.Get (src));
      apps.Start (Seconds (50.0 + (double)i/100.0));
      apps.Stop (Seconds (simulationTime));
    }

  Simulator::Stop (Seconds (simulationTime));

  FlowMonitorHelper flowmon;
  if (enableFlowMonitor)
    {
      flowmon.InstallAll ();
    }

  Simulator::Run ();

  if (enableFlowMonitor)
    {
      Ptr<FlowMonitor> monitor = flowmon.GetMonitor ();
      std::ofstream statsFile;
      statsFile.open ("manet_routing_stats.csv");
      statsFile << "FlowId,SrcAddr,DestAddr,TxPackets,RxPackets,LostPackets,Throughput(kbps)" << std::endl;

      for (FlowMonitor::FlowIdToFlowEntryContainer::const_iterator i = monitor->GetFlows ().begin (); i != monitor->GetFlows ().end (); ++i)
        {
          Ipv4FlowClassifier::FiveTuple t = i->second.flowClassifier.GetFiveTuple ();
          statsFile << i->first << ","
                    << t.sourceAddress << ","
                    << t.destinationAddress << ","
                    << i->second.txPackets << ","
                    << i->second.rxPackets << ","
                    << i->second.lostPackets << ","
                    << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1000
                    << std::endl;
        }
      statsFile.close ();
    }

  Simulator::Destroy ();
  return 0;
}