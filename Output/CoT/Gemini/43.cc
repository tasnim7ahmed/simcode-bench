#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/command-line.h"
#include "ns3/config-store.h"
#include "ns3/netanim-module.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

int main (int argc, char *argv[])
{
  bool enableRtsCts = false;
  uint32_t numAggregatedMpdu = 1;
  uint32_t payloadSize = 1472;
  double simulationTime = 10.0;
  double expectedThroughputMbps = 50.0;
  bool enablePcapTracing = false;
  bool enableAsciiTracing = false;

  CommandLine cmd;
  cmd.AddValue ("enableRtsCts", "Enable RTS/CTS (0 or 1)", enableRtsCts);
  cmd.AddValue ("numAggregatedMpdu", "Number of aggregated MPDUs", numAggregatedMpdu);
  cmd.AddValue ("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("expectedThroughput", "Expected throughput in Mbps", expectedThroughputMbps);
  cmd.AddValue ("enablePcapTracing", "Enable PCAP tracing (0 or 1)", enablePcapTracing);
  cmd.AddValue ("enableAsciiTracing", "Enable ASCII tracing (0 or 1)", enableAsciiTracing);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::WifiMacQueue::MaxAmpduSize", UintegerValue (65535));
  Config::SetDefault ("ns3::WifiMacQueue::MaxAmsduSize", UintegerValue (7935));
  Config::SetDefault ("ns3::WifiMacQueue::EnableAmpdu", BooleanValue (true));
  Config::SetDefault ("ns3::WifiMacQueue::EnableAmsdu", BooleanValue (true));
  Config::SetDefault ("ns3::WifiMacQueue::FragmentationThreshold", UintegerValue (2346));
  Config::SetDefault ("ns3::WifiMacQueue::ShortRetryLimit", UintegerValue (7));
  Config::SetDefault ("ns3::WifiMacQueue::LongRetryLimit", UintegerValue (4));

  NodeContainer staNodes;
  staNodes.Create (2);
  NodeContainer apNode;
  apNode.Create (1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());
  phy.Set ("TxPowerStart", DoubleValue (16.0206));
  phy.Set ("TxPowerEnd", DoubleValue (16.0206));
  phy.Set ("RxSensitivity", DoubleValue (-85.0));
  phy.Set ("CcaEdThreshold", DoubleValue (-82.0));
  phy.Set ("EnergyDetectionThreshold", DoubleValue (-82.0));

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n);

  NqosWifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-ssid");

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, staNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid),
               "BeaconGeneration", BooleanValue (true),
               "BeaconInterval", TimeValue (Seconds (0.1)));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, apNode);

  if (enableRtsCts)
    {
      Config::SetDefault ("ns3::WifiMacQueue::RtsCtsThreshold", UintegerValue (0));
    }

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (2.0),
                                 "DeltaY", DoubleValue (2.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  NodeContainer allNodes;
  allNodes.Add (staNodes);
  allNodes.Add (apNode);
  mobility.Install (allNodes);
  
  Ptr<ConstantPositionMobilityModel> apMobility = apNode.Get(0)->GetObject<ConstantPositionMobilityModel>();
  apMobility->SetPosition(Vector(5, 0, 0));
  Ptr<ConstantPositionMobilityModel> staMobility1 = staNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
  staMobility1->SetPosition(Vector(0, 0, 0));
  Ptr<ConstantPositionMobilityModel> staMobility2 = staNodes.Get(1)->GetObject<ConstantPositionMobilityModel>();
  staMobility2->SetPosition(Vector(0, 2, 0));

  InternetStackHelper stack;
  stack.Install (allNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces;
  staInterfaces = address.Assign (staDevices);
  Ipv4InterfaceContainer apInterfaces;
  apInterfaces = address.Assign (apDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 9;

  UdpServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (apNode.Get (0));
  serverApps.Start (Seconds (0.0));
  serverApps.Stop (Seconds (simulationTime + 1));

  UdpClientHelper echoClient (apInterfaces.GetAddress (0), port);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (0xFFFFFFFF));
  echoClient.SetAttribute ("Interval", TimeValue (Time ("0.00001")));
  echoClient.SetAttribute ("PacketSize", UintegerValue (payloadSize));

  ApplicationContainer clientApps;
  clientApps.Add (echoClient.Install (staNodes.Get (0)));
  clientApps.Add (echoClient.Install (staNodes.Get (1)));
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (simulationTime + 1));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  if (enablePcapTracing)
    {
      phy.EnablePcap ("hidden-node", apDevices);
      phy.EnablePcap ("hidden-node", staDevices);
    }

  if (enableAsciiTracing)
    {
        AsciiTraceHelper ascii;
        phy.EnableAsciiAll (ascii.CreateFileStream ("hidden-node.tr"));
    }

  Simulator::Stop (Seconds (simulationTime + 2));

  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  double totalReceivedBytes = 0;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      if (t.destinationAddress == apInterfaces.GetAddress (0))
        {
          totalReceivedBytes += i->second.rxBytes;
        }
    }

  double throughputMbps = (totalReceivedBytes * 8) / (simulationTime * 1000000.0);

  std::cout << "Throughput: " << throughputMbps << " Mbps" << std::endl;

  if (throughputMbps < expectedThroughputMbps * 0.9)
    {
      std::cerr << "ERROR: Expected throughput not achieved!" << std::endl;
      exit (1);
    }

  monitor->SerializeToXmlFile("hidden-node.flowmon", true, true);

  Simulator::Destroy ();

  return 0;
}