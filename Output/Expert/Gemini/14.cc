#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/command-line.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

int main(int argc, char *argv[]) {
  bool enablePcap = false;
  uint32_t payloadSize = 1472;
  double distance = 1.0;
  bool rtsCts = false;
  double simulationTime = 10.0;
  std::string rate ("5Mbps");

  CommandLine cmd;
  cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue("distance", "Distance between nodes", distance);
  cmd.AddValue("rtsCts", "Enable RTS/CTS", rtsCts);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("rate", "Data rate", rate);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue("2200"));
  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue(rtsCts ? "0" : "2200"));

  NodeContainer apNodes[4], staNodes[4];
  for (int i = 0; i < 4; ++i) {
    apNodes[i].Create(1);
    staNodes[i].Create(1);
  }

  YansWifiChannelHelper channelHelper[4];
  YansWifiPhyHelper phyHelper[4];
  WifiHelper wifiHelper[4];

  for (int i = 0; i < 4; ++i) {
    channelHelper[i] = YansWifiChannelHelper::Default();
    channelHelper[i].AddPropagationLoss("ns3::LogDistancePropagationLossModel");
    channelHelper[i].AddPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");

    phyHelper[i] = YansWifiPhyHelper::Default();
    phyHelper[i].SetChannel(channelHelper[i].Create());
    phyHelper[i].Set("ChannelNumber", UintegerValue(i + 1));

    wifiHelper[i] = WifiHelper::Default();
    wifiHelper[i].SetStandard(WIFI_PHY_STANDARD_80211n);
  }

  WifiMacHelper macHelper[4];
  NetDeviceContainer apDevices[4], staDevices[4];
  for (int i = 0; i < 4; ++i) {
    Ssid ssid = Ssid("ns3-wifi-" + std::to_string(i));
    macHelper[i].SetType("ns3::ApWifiMac",
                          "Ssid", SsidValue(ssid),
                          "BeaconInterval", TimeValue(Seconds(0.1)));

    apDevices[i] = wifiHelper[i].Install(phyHelper[i], macHelper[i], apNodes[i]);

    macHelper[i].SetType("ns3::StaWifiMac",
                          "Ssid", SsidValue(ssid),
                          "ActiveProbing", BooleanValue(false));
    staDevices[i] = wifiHelper[i].Install(phyHelper[i], macHelper[i], staNodes[i]);
  }

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(4),
                                "LayoutType", StringValue("RowFirst"));

  NodeContainer allNodes;
  for (int i = 0; i < 4; ++i) {
      allNodes.Add(apNodes[i]);
      allNodes.Add(staNodes[i]);
  }

  mobility.Install(allNodes);

  InternetStackHelper stack;
  for (int i = 0; i < 4; ++i) {
    stack.Install(apNodes[i]);
    stack.Install(staNodes[i]);
  }

  Ipv4AddressHelper address;
  Ipv4InterfaceContainer apInterfaces[4], staInterfaces[4];
  for (int i = 0; i < 4; ++i) {
      address.SetBase("10.1." + std::to_string(i) + ".0", "255.255.255.0");
      staInterfaces[i] = address.Assign(staDevices[i]);
      apInterfaces[i] = address.Assign(apDevices[i]);
      address.NewNetwork();
  }

  UdpServerHelper server(9);
  ApplicationContainer serverApps[4];
  for (int i = 0; i < 4; ++i) {
      serverApps[i] = server.Install(apNodes[i]);
      serverApps[i].Start(Seconds(1.0));
      serverApps[i].Stop(Seconds(simulationTime));
  }

  OnOffHelper onoff[4];
  ApplicationContainer clientApps[4];
  for (int i = 0; i < 4; ++i) {
      onoff[i].SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
      onoff[i].SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
      onoff[i].SetAttribute("PacketSize", UintegerValue(payloadSize));
      onoff[i].SetAttribute("DataRate", DataRateValue(DataRate(rate)));
  }

  for (int i = 0; i < 4; ++i) {
    TrafficControlLayer::QosTag qosTag;
    if(i % 2 == 0){
        qosTag.SetTid(0);
    } else {
        qosTag.SetTid(6);
    }

      onoff[i].SetAttribute("Protocol", StringValue("ns3::UdpSocketFactory"));
      AddressValue remoteAddress(InetSocketAddress(apInterfaces[i].GetAddress(0), 9));
      onoff[i].SetAttribute("Remote", remoteAddress);
      clientApps[i] = onoff[i].Install(staNodes[i]);
      clientApps[i].Start(Seconds(2.0));
      clientApps[i].Stop(Seconds(simulationTime));
  }

  if (enablePcap) {
    phyHelper[0].EnablePcapAll("wifi-qos");
  }

  Simulator::Stop(Seconds(simulationTime));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
    std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
  }

  Simulator::Destroy();
  return 0;
}