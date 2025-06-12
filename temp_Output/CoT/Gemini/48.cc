#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/spectrum-wifi-phy.h"
#include "ns3/yans-wifi-phy.h"
#include "ns3/command-line.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/tcp-client-server-helper.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ssid.h"
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiExperiment");

int main(int argc, char *argv[]) {
  bool enablePcap = false;
  double simulationTime = 10.0;
  double distance = 10.0;
  int mcsIndex = 7;
  bool useSpectrum = false;
  bool useUdp = true;
  int channelWidth = 20;
  bool useShortGuardInterval = false;
  std::string phyMode = "OfdmRate6Mbps";
  std::string rate = "5Mbps";
  uint16_t port = 9;

  CommandLine cmd;
  cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("distance", "Distance between nodes in meters", distance);
  cmd.AddValue("mcsIndex", "MCS index (0-7 for 802.11n)", mcsIndex);
  cmd.AddValue("useSpectrum", "Use SpectrumWifiPhy instead of YansWifiPhy", useSpectrum);
  cmd.AddValue("useUdp", "Use UDP traffic instead of TCP", useUdp);
  cmd.AddValue("channelWidth", "Channel width (20 or 40 MHz)", channelWidth);
  cmd.AddValue("useShortGuardInterval", "Use short guard interval", useShortGuardInterval);
  cmd.AddValue("phyMode", "Wifi Phy rate", phyMode);
  cmd.AddValue("rate", "Application data rate", rate);
  cmd.Parse(argc, argv);

  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create(2);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  if (useSpectrum) {
        phy = YansWifiPhyHelper::Default();
  }
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, nodes.Get(1));

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "BeaconGeneration", BooleanValue(true),
              "BeaconInterval", TimeValue(Seconds(0.1)));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, nodes.Get(0));

  if (channelWidth == 40) {
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue(40));
  }

  if (useShortGuardInterval) {
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ShortGuardIntervalSupported", BooleanValue(true));
  }

  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue(phyMode),
                               "ControlMode", StringValue(phyMode));

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(staDevices);
  address.Assign(apDevices);

  Ptr<Node> appSource = nodes.Get(1);
  Ptr<Node> appSink = nodes.Get(0);

  if (useUdp) {
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(appSink);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime + 1));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(4294967295));
    echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(appSource);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime));
  } else {
    TcpEchoServerHelper echoServer(port);
        ApplicationContainer serverApps = echoServer.Install(appSink);
        serverApps.Start (Seconds (1.0));
        serverApps.Stop (Seconds (simulationTime + 1));

        TcpEchoClientHelper echoClient (interfaces.GetAddress (0), port);
        echoClient.SetAttribute ("MaxPackets", UintegerValue (4294967295));
        echoClient.SetAttribute ("Interval", TimeValue (MilliSeconds (1)));
        echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
        ApplicationContainer clientApps = echoClient.Install (appSource);
        clientApps.Start (Seconds (2.0));
        clientApps.Stop (Seconds (simulationTime));
  }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  if (enablePcap) {
      phy.EnablePcap("wifi-experiment", apDevices.Get(0));
  }

  Simulator::Stop(Seconds(simulationTime + 1));

  Simulator::Run();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  double throughput = 0.0;

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      NS_LOG_UNCOND("Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")");
      NS_LOG_UNCOND("  Tx Packets:   " << i->second.txPackets);
      NS_LOG_UNCOND("  Rx Packets:   " << i->second.rxPackets);
      NS_LOG_UNCOND("  Lost Packets: " << i->second.lostPackets);
      NS_LOG_UNCOND("  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps");
      throughput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
    }

  Simulator::Destroy();

  std::cout << "Simulation Completed" << std::endl;
  std::cout << "Throughput: " << throughput << " Mbps" << std::endl;

  return 0;
}