#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("HybridNetwork");

int main (int argc, char *argv[])
{
  bool tracing = true;
  double simulationTime = 10.0;
  std::string dataRate = "5Mbps";
  std::string delay = "2ms";

  CommandLine cmd;
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.Parse (argc, argv);

  NodeContainer wiredNodes;
  wiredNodes.Create (4);

  NodeContainer wifiNodes;
  wifiNodes.Create (3);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue (dataRate));
  pointToPoint.SetChannelAttribute ("Delay", StringValue (delay));

  NetDeviceContainer p2pDevices;
  for (uint32_t i = 0; i < wiredNodes.GetN () - 1; ++i)
    {
      NetDeviceContainer link = pointToPoint.Install (wiredNodes.Get (i), wiredNodes.Get (i + 1));
      p2pDevices.Add (link);
    }

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-ssid");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices = wifi.Install (phy, mac, wifiNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices = wifi.Install (phy, mac, wiredNodes.Get (0));

  InternetStackHelper stack;
  stack.Install (wiredNodes);
  stack.Install (wifiNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces;
  for (uint32_t i = 0; i < wiredNodes.GetN () - 1; ++i)
    {
      NetDeviceContainer link;
      link.Add (p2pDevices.Get (i * 2));
      link.Add (p2pDevices.Get (i * 2 + 1));
      Ipv4InterfaceContainer interface = address.Assign (link);
      p2pInterfaces.Add (interface);
      address.NewNetwork ();
    }

  address.SetBase ("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer wifiInterfaces = address.Assign (staDevices);
  Ipv4InterfaceContainer apInterface = address.Assign (apDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  UdpEchoServerHelper echoServer (9);

  ApplicationContainer serverApps = echoServer.Install (wiredNodes.Get (3));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime));

  uint16_t port = 9;
  for (uint32_t i = 0; i < wifiNodes.GetN (); ++i)
    {
      UdpEchoClientHelper echoClient (p2pInterfaces.GetAddress (p2pInterfaces.GetN ()-1, 0), port);
      echoClient.SetAttribute ("MaxPackets", UintegerValue (10000));
      echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
      echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
      ApplicationContainer clientApps = echoClient.Install (wifiNodes.Get (i));
      clientApps.Start (Seconds (2.0));
      clientApps.Stop (Seconds (simulationTime));
    }

  BulkSendHelper bulkSendHelper ("ns3::TcpSocketFactory",
                                 InetSocketAddress (p2pInterfaces.GetAddress (p2pInterfaces.GetN ()-1, 0), 50000));

  bulkSendHelper.SetAttribute ("SendSize", UintegerValue (1024));

  for (uint32_t i = 0; i < wifiNodes.GetN (); ++i)
  {
    ApplicationContainer bulkSendApp = bulkSendHelper.Install (wifiNodes.Get (i));
    bulkSendApp.Start (Seconds (3.0));
    bulkSendApp.Stop (Seconds (simulationTime));
  }

  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory",
                                     InetSocketAddress (Ipv4Address::GetAny (), 50000));
  ApplicationContainer sinkApp = packetSinkHelper.Install (wiredNodes.Get (3));
  sinkApp.Start (Seconds (3.0));
  sinkApp.Stop (Seconds (simulationTime));

  if (tracing == true)
    {
      phy.EnablePcapAll ("hybrid");
    }

  Simulator::Stop (Seconds (simulationTime));

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll ();

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowMonitor.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  double totalBytes = 0.0;
  Time firstTxTime;
  Time lastRxTime;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      if (t.destinationAddress == p2pInterfaces.GetAddress (p2pInterfaces.GetN ()-1, 0) && t.destinationPort == 50000)
        {
          totalBytes += i->second.txBytes;
          firstTxTime = i->second.timeFirstTxPacket;
          lastRxTime = i->second.timeLastRxPacket;
        }
    }

  double throughput = (totalBytes * 8.0) / (lastRxTime.GetSeconds() - firstTxTime.GetSeconds())/1000000;

  std::cout << "Average throughput: " << throughput << " Mbps" << std::endl;

  Simulator::Destroy ();
  return 0;
}