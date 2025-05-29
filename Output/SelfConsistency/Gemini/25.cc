#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/config-store-module.h"
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Wifi80211axSimulation");

int main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t nSta = 1;
  std::string phyMode ("HeMcs7");
  double distance = 10.0;
  bool rtscts = false;
  bool enable8080 = false;
  bool enableExtBa = false;
  bool ulOfdma = false;
  std::string band = "5GHz";
  uint32_t packetSize = 1472;
  std::string rate ("50Mbps");
  double simulationTime = 10.0;
  std::string transportProtocol = "Udp";
  double targetThroughputMbps = 50.0;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("nSta", "Number of stations", nSta);
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("distance", "Distance between AP and station", distance);
  cmd.AddValue ("rtscts", "Enable RTS/CTS", rtscts);
  cmd.AddValue ("enable8080", "Enable 80+80 MHz channel", enable8080);
  cmd.AddValue ("enableExtBa", "Enable extended Block Ack", enableExtBa);
  cmd.AddValue ("ulOfdma", "Enable UL OFDMA", ulOfdma);
  cmd.AddValue ("band", "Frequency band (2.4GHz, 5GHz, 6GHz)", band);
  cmd.AddValue ("packetSize", "Size of packets", packetSize);
  cmd.AddValue ("rate", "CBR traffic rate", rate);
  cmd.AddValue ("simulationTime", "Simulation time", simulationTime);
  cmd.AddValue ("transportProtocol", "Transport protocol (Udp, Tcp)", transportProtocol);
  cmd.AddValue ("targetThroughputMbps", "Target throughput in Mbps", targetThroughputMbps);

  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("Wifi80211axSimulation", LOG_LEVEL_INFO);
    }

  Config::SetDefault ("ns3::WifiMacQueue::MaxSize", StringValue ("1000000"));
  Config::SetDefault ("ns3::WifiMacQueue::Mode", EnumValue (WifiMacQueue::ADAPTIVE));

  NodeContainer staNodes;
  staNodes.Create (nSta);
  NodeContainer apNode;
  apNode.Create (1);

  WifiHelper wifiHelper;
  wifiHelper.SetStandard (WIFI_PHY_STANDARD_80211ax);

  YansWifiChannelHelper channel;
  channel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  channel.AddPropagationLoss ("ns3::FriisPropagationLossModel");

  YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default ();
  phyHelper.SetChannel (channel.Create ());

  // Set band
  if (band == "2.4GHz")
    {
      phyHelper.Set ("ChannelNumber", UintegerValue (1));
    }
  else if (band == "5GHz")
    {
      phyHelper.Set ("ChannelNumber", UintegerValue (36));
    }
  else if (band == "6GHz")
    {
      phyHelper.Set ("ChannelNumber", UintegerValue (1));  // Example, adjust as needed
    }
  else
    {
      std::cerr << "Invalid band specified: " << band << std::endl;
      return 1;
    }


  wifiHelper.SetRemoteStationManager ("ns3::HeWifiMac",
                                       "Ssid", SsidValue (Ssid ("ns3-80211ax")),
                                       "HeTargetTidPreferenceDisable", BooleanValue (false),
                                       "HeMuBaPreferenceDisable", BooleanValue (false),
                                       "AmsduMaxSize", UintegerValue (7935),
                                       "HtSupported", BooleanValue (false),
                                       "VhtSupported", BooleanValue (false),
                                       "HeSupported", BooleanValue (true),
                                       "HeMcs", StringValue (phyMode));


  WifiMacHelper macHelper;

  Ssid ssid = Ssid ("ns3-80211ax");
  macHelper.SetType ("ns3::StaWifiMac",
                     "Ssid", SsidValue (ssid),
                     "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifiHelper.Install (phyHelper, macHelper, staNodes);

  macHelper.SetType ("ns3::ApWifiMac",
                     "Ssid", SsidValue (ssid),
                     "BeaconGeneration", BooleanValue (true),
                     "QosSupported", BooleanValue (true),
                     "HtSupported", BooleanValue (false),
                     "VhtSupported", BooleanValue (false),
                     "HeSupported", BooleanValue (true));

  NetDeviceContainer apDevices;
  apDevices = wifiHelper.Install (phyHelper, macHelper, apNode);


  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (distance),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (nSta),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (staNodes);

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (distance + 10),
                                 "DeltaX", DoubleValue (0.0),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (1),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.Install (apNode);

  InternetStackHelper stack;
  stack.Install (apNode);
  stack.Install (staNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staNodeIface;
  Ipv4InterfaceContainer apNodeIface;
  staNodeIface = address.Assign (staDevices);
  apNodeIface = address.Assign (apDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;

  ApplicationContainer appContainer;
  for (uint32_t i = 0; i < nSta; ++i)
    {
      if (transportProtocol == "Udp")
        {
          UdpClientHelper client (apNodeIface.GetAddress (0), port);
          client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
          client.SetAttribute ("Interval", TimeValue (Seconds (packetSize * 8 / (double)targetThroughputMbps / 1e6))); // Adjust interval for target throughput
          client.SetAttribute ("PacketSize", UintegerValue (packetSize));
          appContainer.Add (client.Install (staNodes.Get (i)));
        }
      else if (transportProtocol == "Tcp")
        {
          TcpClientHelper client (apNodeIface.GetAddress (0), port);
          client.SetAttribute ("MaxPackets", UintegerValue (4294967295));
          client.SetAttribute ("PacketSize", UintegerValue (packetSize));
          appContainer.Add (client.Install (staNodes.Get (i)));

          TcpServerHelper server (port);
          ApplicationContainer serverApps = server.Install (apNode.Get (0));
          serverApps.Start (Seconds (0.0));
          serverApps.Stop (Seconds (simulationTime + 1));

        }
    }


  if (transportProtocol == "Udp")
    {
      UdpServerHelper echoServer (port);
      ApplicationContainer serverApps = echoServer.Install (apNode.Get (0));
      serverApps.Start (Seconds (0.0));
      serverApps.Stop (Seconds (simulationTime + 1));
    }

  appContainer.Start (Seconds (1.0));
  appContainer.Stop (Seconds (simulationTime + 1));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simulationTime + 2));

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  double totalThroughput = 0.0;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1000000 << " Mbps\n";
      totalThroughput += i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds () - i->second.timeFirstTxPacket.GetSeconds ()) / 1000000;
    }

  std::cout << "Total throughput: " << totalThroughput << " Mbps" << std::endl;

  monitor->SerializeToXmlFile ("80211ax-simulation.flowmon", true, true);

  Simulator::Destroy ();

  return 0;
}