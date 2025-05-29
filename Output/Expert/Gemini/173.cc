#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("HybridNetwork");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer wiredNodes;
  wiredNodes.Create (4);

  NodeContainer wirelessNodes;
  wirelessNodes.Create (3);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer p2pDevices1 = p2p.Install (wiredNodes.Get (0), wiredNodes.Get (1));
  NetDeviceContainer p2pDevices2 = p2p.Install (wiredNodes.Get (1), wiredNodes.Get (2));
  NetDeviceContainer p2pDevices3 = p2p.Install (wiredNodes.Get (2), wiredNodes.Get (3));

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-wifi");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices = wifi.Install (phy, mac, wirelessNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices = wifi.Install (phy, mac, wiredNodes.Get (0));

  InternetStackHelper stack;
  stack.Install (wiredNodes);
  stack.Install (wirelessNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces1 = address.Assign (p2pDevices1);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces2 = address.Assign (p2pDevices2);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces3 = address.Assign (p2pDevices3);

  address.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer wifiInterfaces = address.Assign (staDevices);
  address.Assign (apDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 50000;
  Address sinkAddress (InetSocketAddress (p2pInterfaces3.GetAddress (1), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", sinkAddress);
  ApplicationContainer sinkApps = packetSinkHelper.Install (wiredNodes.Get (3));
  sinkApps.Start (Seconds (1.0));
  sinkApps.Stop (Seconds (10.0));

  BulkSendHelper source ("ns3::TcpSocketFactory", Address (sinkAddress));
  source.SetAttribute ("SendSize", UintegerValue (1400));

  ApplicationContainer sourceApps;
  for (uint32_t i = 0; i < wirelessNodes.GetN (); ++i)
  {
    sourceApps.Add (source.Install (wirelessNodes.Get (i)));
  }

  sourceApps.Start (Seconds (2.0));
  sourceApps.Stop (Seconds (9.0));

  Simulator::Schedule (Seconds(0.00001), [](){ PcapHelper pcapHelper; pcapHelper.EnablePcapAll("hybrid_network");});

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll ();

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowMonitor.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  double totalBytes = 0.0;
  double totalTime = 0.0;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      if (t.destinationAddress == p2pInterfaces3.GetAddress (1))
        {
          totalBytes += i->second.bytesRx;
          totalTime += i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds();
        }
    }
  double throughput = (totalBytes * 8.0) / (totalTime * 1000000);

  std::cout << "Average Throughput: " << throughput << " Mbps" << std::endl;
  monitor->SerializeToXmlFile("hybrid_network.flowmon", true, true);

  Simulator::Destroy ();
  return 0;
}