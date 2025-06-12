#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ThreeNodeLineWifiThroughput");

void
TracePacketTx (Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet)
{
  *stream->GetStream() << Simulator::Now ().GetSeconds () << ",TX," << packet->GetSize () << std::endl;
}

void
TracePacketRx (Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet, const Address &address)
{
  *stream->GetStream() << Simulator::Now ().GetSeconds () << ",RX," << packet->GetSize () << std::endl;
}

int
main (int argc, char *argv[])
{
  double simulationTime = 10.0;
  std::string filename = "packet-trace.csv";
  std::string phyMode ("HtMcs7");
  uint32_t packetSize = 1024;
  double dataRate = 1e6; // 1 Mbps
  double interval = double (packetSize * 8) / dataRate;

  CommandLine cmd;
  cmd.AddValue ("simulationTime", "Simulation time (s)", simulationTime);
  cmd.AddValue ("filename", "Trace file name", filename);
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (3);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211n_2_4GHZ);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue (phyMode),
                               "ControlMode", StringValue (phyMode));
  WifiMacHelper mac;
  Ssid ssid = Ssid ("wifi-simple");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices.Add (wifi.Install (phy, mac, nodes.Get (0)));
  staDevices.Add (wifi.Install (phy, mac, nodes.Get (2)));

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice;
  apDevice = wifi.Install (phy, mac, nodes.Get (1));

  NetDeviceContainer devices;
  devices.Add (staDevices);
  devices.Add (apDevice);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (50.0, 0.0, 0.0));
  positionAlloc->Add (Vector (100.0, 0.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  uint16_t sinkPort = 8080;
  Address sinkAddress (InetSocketAddress (interfaces.GetAddress (2), sinkPort));
  PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApp = packetSinkHelper.Install (nodes.Get (2));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (simulationTime + 1));

  OnOffHelper cbrHelper ("ns3::UdpSocketFactory", sinkAddress);
  cbrHelper.SetAttribute ("PacketSize", UintegerValue (packetSize));
  cbrHelper.SetAttribute ("DataRate", DataRateValue (DataRate (int(dataRate))));
  cbrHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  cbrHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

  ApplicationContainer sourceApp = cbrHelper.Install (nodes.Get (0));
  sourceApp.Start (Seconds (1.0));
  sourceApp.Stop (Seconds (simulationTime + 1));

  // Enable Pcap Tracing if needed
  phy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
  phy.EnablePcap ("three-node-line", devices);

  // Custom tracing for transmitted and received packets
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream (filename);

  Config::ConnectWithoutContext (
    "/NodeList/0/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyTxBegin",
    MakeBoundCallback (&TracePacketTx, stream));
  Config::ConnectWithoutContext (
    "/NodeList/2/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxEnd",
    MakeBoundCallback (&TracePacketRx, stream));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simulationTime + 2));
  Simulator::Run ();

  // Throughput calculation
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());

  double totalBytes = 0;
  for (auto stats = monitor->GetFlowStats ().begin ();
       stats != monitor->GetFlowStats ().end ();
       ++stats)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (stats->first);
      if ((t.sourceAddress == interfaces.GetAddress (0) && t.destinationAddress == interfaces.GetAddress (2)) ||
          (t.sourceAddress == interfaces.GetAddress (2) && t.destinationAddress == interfaces.GetAddress (0)))
        {
          totalBytes += stats->second.rxBytes;
        }
    }
  double throughput = (totalBytes * 8) / (simulationTime * 1e6);
  std::ofstream thfile;
  thfile.open ("throughput.txt");
  thfile << "End-to-end throughput: " << throughput << " Mbps" << std::endl;
  thfile.close ();

  Simulator::Destroy ();
  return 0;
}