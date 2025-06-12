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
#include <string>
#include <vector>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Wifi7Throughput");

int main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t nsta = 1;
  std::string trafficType = "TCP";
  uint32_t payloadSize = 1472;
  uint32_t mpduBufferSize = 8192;
  uint32_t mcs = 7;
  uint32_t channelWidth = 80;
  std::string guardInterval = "0.8us";
  std::string frequency = "5GHz";
  bool uplinkOfdma = false;
  bool bsrp = false;
  double simulationTime = 10.0;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log ifconfig traces to stdout", verbose);
  cmd.AddValue ("nsta", "Number of wifi stations", nsta);
  cmd.AddValue ("trafficType", "Traffic type (TCP or UDP)", trafficType);
  cmd.AddValue ("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue ("mpduBufferSize", "MPDU buffer size in bytes", mpduBufferSize);
  cmd.AddValue ("mcs", "Modulation and Coding Scheme (MCS)", mcs);
  cmd.AddValue ("channelWidth", "Channel width (MHz)", channelWidth);
  cmd.AddValue ("guardInterval", "Guard interval (0.8us, 1.6us, 3.2us)", guardInterval);
  cmd.AddValue ("frequency", "Operating frequency (2.4GHz, 5GHz, 6GHz)", frequency);
  cmd.AddValue ("uplinkOfdma", "Enable uplink OFDMA", uplinkOfdma);
  cmd.AddValue ("bsrp", "Enable BSRP", bsrp);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);

  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("Wifi7Throughput", LOG_LEVEL_ALL);
    }

  Config::SetDefault ("ns3::WifiMacQueue::MaxMPDU", UintegerValue (mpduBufferSize));

  NodeContainer staNodes;
  staNodes.Create (nsta);
  NodeContainer apNode;
  apNode.Create (1);

  YansWifi7PhyHelper phy = YansWifi7PhyHelper::Default ();
  phy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper::ChannelParams params;
  double frequencyValue;
  if (frequency == "2.4GHz") {
      frequencyValue = 2.4e9;
  } else if (frequency == "5GHz") {
      frequencyValue = 5.0e9;
  } else if (frequency == "6GHz") {
      frequencyValue = 6.0e9;
  } else {
      std::cerr << "Invalid frequency specified. Supported values are 2.4GHz, 5GHz, and 6GHz." << std::endl;
      return 1;
  }
  params.Add ("Frequency", DoubleValue (frequencyValue));
  channel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  channel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  phy.SetChannel (channel.Create (params));

  Wifi7MacHelper mac;
  Ssid ssid = Ssid ("ns3-wifi7");

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211be);

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices = wifi.Install (phy, mac, staNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid),
               "BeaconGeneration", BooleanValue (true),
               "UplinkOfdma", BooleanValue (uplinkOfdma),
               "Bsrp", BooleanValue (bsrp));

  NetDeviceContainer apDevices = wifi.Install (phy, mac, apNode);

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                               "MinX", DoubleValue (0.0),
                               "MinY", DoubleValue (0.0),
                               "DeltaX", DoubleValue (5.0),
                               "DeltaY", DoubleValue (10.0),
                               "GridWidth", UintegerValue (3),
                               "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  mobility.Install (staNodes);
  mobility.Install (apNode);

  InternetStackHelper stack;
  stack.Install (apNode);
  stack.Install (staNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staNodeInterface;
  Ipv4InterfaceContainer apNodeInterface;
  apNodeInterface = address.Assign (apDevices);
  address.Assign (staDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 50000;

  ApplicationContainer sourceApps;
  ApplicationContainer sinkApps;

  for (uint32_t i = 0; i < nsta; ++i)
    {
      Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
      PacketSinkHelper packetSinkHelper (sinkLocalAddress);
      sinkApps.Add (packetSinkHelper.Install (staNodes.Get (i)));

      InetSocketAddress sourceAddress (apNodeInterface.GetAddress (0), port);
      sourceAddress.SetTos (0xb8);

      if (trafficType == "TCP")
        {
          BulkSendHelper bulkSendHelper ("ns3::TcpSocketFactory", sourceAddress);
          bulkSendHelper.SetAttribute ("SendSize", UintegerValue (payloadSize));
          sourceApps.Add (bulkSendHelper.Install (apNode.Get (0)));
        }
      else if (trafficType == "UDP")
        {
          UdpClientHelper udpClientHelper (sourceAddress);
          udpClientHelper.SetAttribute ("PacketSize", UintegerValue (payloadSize));
          udpClientHelper.SetAttribute ("MaxPackets", UintegerValue (1000000));
          sourceApps.Add (udpClientHelper.Install (apNode.Get (0)));
        }
      else
        {
          std::cerr << "Invalid traffic type specified. Supported values are TCP and UDP." << std::endl;
          return 1;
        }
      ++port;
    }

  sinkApps.Start (Seconds (1.0));
  sourceApps.Start (Seconds (1.0));

  FlowMonitorHelper flowMonitorHelper;
  Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll ();

  Simulator::Stop (Seconds (simulationTime + 1));

  Simulator::Run ();

  flowMonitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowMonitorHelper.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats ();

  double totalRxBytes = 0.0;
  for (auto const& flow : stats)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (flow.first);
      if (t.destinationAddress == apNodeInterface.GetAddress (0))
        {
          totalRxBytes += flow.second.rxBytes;
        }
    }

  double throughputMbps = (totalRxBytes * 8.0) / (simulationTime * 1000000.0);

  std::cout << "MCS: " << mcs << ", Channel Width: " << channelWidth << "MHz, GI: " << guardInterval
            << ", Throughput: " << std::fixed << std::setprecision (2) << throughputMbps << " Mbps" << std::endl;

  double expectedMinThroughput = 0.0;
  double expectedMaxThroughput = 0.0;

  if (mcs == 7 && channelWidth == 80 && guardInterval == "0.8us") {
      expectedMinThroughput = 100.0;
      expectedMaxThroughput = 300.0;
  }
  else {
      expectedMinThroughput = 50.0;
      expectedMaxThroughput = 200.0;
  }

  double tolerance = 0.1;

  if (throughputMbps < expectedMinThroughput * (1 - tolerance) || throughputMbps > expectedMaxThroughput * (1 + tolerance)) {
      std::cerr << "ERROR: Throughput is outside the expected range for MCS " << mcs
                << ", Channel Width " << channelWidth << "MHz, and GI " << guardInterval << std::endl;
      Simulator::Destroy ();
      return 1;
  }

  flowMonitor->SerializeToXmlFile ("wifi7_throughput.xml", false, true);

  Simulator::Destroy ();
  return 0;
}