#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SensorNetworkSimulation");

int main (int argc, char *argv[])
{
  bool enablePcap = false;
  std::string animFile = "sensor-network.xml";

  CommandLine cmd;
  cmd.AddValue ("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.AddValue ("animFile", "File Name for Animation Output", animFile);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetDefaultLogLevel (LogLevel::LOG_LEVEL_INFO);
  LogComponent::SetLogLevel ("UdpClient", LogLevel::LOG_LEVEL_ALL);
  LogComponent::SetLogLevel ("UdpServer", LogLevel::LOG_LEVEL_ALL);

  NodeContainer sinkNode;
  sinkNode.Create (1);

  NodeContainer sensorNodes;
  sensorNodes.Create (5);

  NodeContainer allNodes;
  allNodes.Add (sinkNode);
  allNodes.Add (sensorNodes);

  WifiHelper wifiHelper;
  wifiHelper.SetStandard (WIFI_PHY_STANDARD_802154);

  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiHelper.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                      "DataMode",StringValue ("OfdmRate6Mbps"),
                                      "ControlMode",StringValue ("OfdmRate6Mbps"));

  Ssid ssid = Ssid ("sensor-network");
  wifiMac.SetType ("ns3::StaWifiMac",
                    "Ssid", SsidValue (ssid),
                    "ActiveProbing", BooleanValue (false));

  NetDeviceContainer sensorDevices = wifiHelper.Install (wifiPhy, wifiMac, sensorNodes);

  wifiMac.SetType ("ns3::ApWifiMac",
                    "Ssid", SsidValue (ssid));

  NetDeviceContainer sinkDevice = wifiHelper.Install (wifiPhy, wifiMac, sinkNode);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (sensorNodes);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (sinkNode);
  Ptr<Node> sink = sinkNode.Get (0);
  Ptr<ConstantPositionMobilityModel> sinkMobility = sink->GetObject<ConstantPositionMobilityModel> ();
  sinkMobility->SetPosition (Vector (10.0, 5.0, 0.0));

  SixLowPanHelper sixLowPanHelper;
  NetDeviceContainer sixLowPanDevices = sixLowPanHelper.Install (allNodes);

  InternetStackHelper internet;
  internet.Install (allNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (sixLowPanDevices);

  UdpServerHelper server (9);
  ApplicationContainer sinkApp = server.Install (sinkNode.Get (0));
  sinkApp.Start (Seconds (1.0));
  sinkApp.Stop (Seconds (10.0));

  UdpClientHelper client (interfaces.GetAddress (0), 9);
  client.SetAttribute ("MaxPackets", UintegerValue (1000));
  client.SetAttribute ("Interval", TimeValue (MilliSeconds (10)));
  client.SetAttribute ("PacketSize", UintegerValue (100));

  ApplicationContainer clientApps;
  for (uint32_t i = 1; i < allNodes.GetN (); ++i)
    {
      clientApps.Add (client.Install (allNodes.Get (i)));
    }

  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  if (enablePcap)
    {
      wifiPhy.EnablePcapAll ("sensor-network");
    }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  AnimationInterface anim (animFile);
  anim.SetConstantPosition (sinkNode.Get (0), 10.0, 5.0);
  for(uint32_t i = 0; i < sensorNodes.GetN(); ++i)
  {
      anim.UpdateNodeColor(sensorNodes.Get(i), 0, 0, 255);
  }
  anim.UpdateNodeColor(sinkNode.Get(0), 255, 0, 0);

  Simulator::Stop (Seconds (11.0));

  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
      std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
      std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
      std::cout << "  Throughput: " << i->second.txBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " kbps\n";
      std::cout << "  End to End Delay: " << i->second.delaySum.GetSeconds()/i->second.rxPackets << "\n";
    }

  Simulator::Destroy ();

  return 0;
}