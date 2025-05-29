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
  std::string phyMode ("DsssRate1Mbps");
  double simulationTime = 10; // seconds
  uint32_t packetSize = 1024;
  uint32_t numWiredNodes = 4;
  uint32_t numWifiNodes = 3;
  uint16_t port = 50000;
  std::string appDataRate = "1Mbps";

  CommandLine cmd;
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("packetSize", "Size of packets generated", packetSize);
  cmd.AddValue ("appDataRate", "Application data rate", appDataRate);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::WifiMac::Ssid", SsidValue (Ssid ("ns-3-ssid")));

  NodeContainer wiredNodes;
  wiredNodes.Create (numWiredNodes);

  NodeContainer wifiNodes;
  wifiNodes.Create (numWifiNodes);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer wiredDevices;
  for (uint32_t i = 0; i < numWiredNodes - 1; ++i)
    {
      NetDeviceContainer link = pointToPoint.Install (wiredNodes.Get (i), wiredNodes.Get (i + 1));
      wiredDevices.Add (link.Get (0));
      wiredDevices.Add (link.Get (1));
    }

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue (phyMode),
                                "ControlMode", StringValue (phyMode));

  NetDeviceContainer wifiDevices = wifi.Install (wifiPhy, wifiMac.Adhoc ("ns-3-ssid"), wifiNodes);

  InternetStackHelper stack;
  stack.Install (wiredNodes);
  stack.Install (wifiNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer wiredInterfaces;
  for (uint32_t i = 0; i < numWiredNodes - 1; ++i)
    {
      Ipv4InterfaceContainer interface = address.Assign (NetDeviceContainer (wiredDevices.Get (2 * i), wiredDevices.Get (2 * i + 1)));
      wiredInterfaces.Add (interface.Get (0));
      wiredInterfaces.Add (interface.Get (1));
      address.NewNetwork ();
    }

  address.SetBase ("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer wifiInterfaces = address.Assign (wifiDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint32_t serverNodeId = 0;

  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApp = sinkHelper.Install (wiredNodes.Get (serverNodeId));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simulationTime));

  for (uint32_t i = 0; i < numWifiNodes; ++i)
    {
      BulkSendHelper sourceHelper ("ns3::TcpSocketFactory", InetSocketAddress (wiredInterfaces.GetAddress (0), port));
      sourceHelper.SetAttribute ("MaxBytes", UintegerValue (0));
      sourceHelper.SetAttribute ("SendSize", UintegerValue (packetSize));
      ApplicationContainer sourceApp = sourceHelper.Install (wifiNodes.Get (i));
      sourceApp.Start (Seconds (1.0));
      sourceApp.Stop (Seconds (simulationTime));
    }

  if (tracing)
    {
       pointToPoint.EnablePcapAll("hybrid");
       wifiPhy.EnablePcap("hybrid", wifiDevices);
    }

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll ();

  Simulator::Stop (Seconds (simulationTime));
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
      if (t.destinationAddress == wiredInterfaces.GetAddress (0))
        {
          totalBytes += i->second.bytesRx;
          if (firstTxTime.IsZero() || i->second.timeFirstTxPacket < firstTxTime) {
            firstTxTime = i->second.timeFirstTxPacket;
          }
          if (lastRxTime.IsZero() || i->second.timeLastRxPacket > lastRxTime) {
            lastRxTime = i->second.timeLastRxPacket;
          }
        }
    }

  Time duration = lastRxTime - firstTxTime;
  double throughput = (totalBytes * 8.0) / duration.GetSeconds() / 1000000;

  std::cout << "Average throughput: " << throughput << " Mbps" << std::endl;

  Simulator::Destroy ();
  return 0;
}