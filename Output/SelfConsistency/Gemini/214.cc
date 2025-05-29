#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"

#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiExample");

int main(int argc, char *argv[]) {
  bool tracing = true;
  uint32_t payloadSize = 1472;
  std::string dataRate = "5Mbps";
  std::string phyMode = "DsssRate11Mbps";

  CommandLine cmd;
  cmd.AddValue("tracing", "Enable or disable packet tracing", tracing);
  cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue("dataRate", "Data rate for TCP agent", dataRate);
  cmd.AddValue("phyMode", "802.11b physical layer mode", phyMode);
  cmd.Parse(argc, argv);

  NodeContainer staNodes;
  staNodes.Create(3);

  NodeContainer apNode;
  apNode.Create(1);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.Set("RxGain", DoubleValue(-10));
  wifiPhy.Set("TxPowerStart", DoubleValue(20));
  wifiPhy.Set("TxPowerEnd", DoubleValue(20));
  wifiPhy.Set("EnergyDetectionThreshold", DoubleValue(-110));
  wifiPhy.Set("CcaEdThreshold", DoubleValue(-80));

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel(wifiChannel.Create());

  WifiMacHelper wifiMac;
  Ssid ssid = Ssid("ns-3-ssid");
  wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid),
                  "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(wifiPhy, wifiMac, staNodes);

  wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(wifiPhy, wifiMac, apNode);

  MobilityHelper mobility;

  mobility.SetPositionAllocator("ns3::GridPositionAllocator", "MinX",
                                DoubleValue(0.0), "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(5.0), "DeltaY",
                                DoubleValue(10.0), "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(staNodes);

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNode);

  InternetStackHelper stack;
  stack.Install(staNodes);
  stack.Install(apNode);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staNodeInterface;
  staNodeInterface = address.Assign(staDevices);
  Ipv4InterfaceContainer apNodeInterface;
  apNodeInterface = address.Assign(apDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 5000;

  UdpClientHelper clientHelper(apNodeInterface.GetAddress(0), port);
  clientHelper.SetAttribute("MaxPackets", UintegerValue(1000));
  clientHelper.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  clientHelper.SetAttribute("PacketSize", UintegerValue(payloadSize));

  ApplicationContainer clientApps = clientHelper.Install(staNodes.Get(0));
  clientApps.Start(Seconds(1.0));
  clientApps.Stop(Seconds(10.0));

  UdpServerHelper serverHelper(port);
  ApplicationContainer serverApps = serverHelper.Install(apNode.Get(0));
  serverApps.Start(Seconds(1.0));
  serverApps.Stop(Seconds(10.0));

  UdpClientHelper clientHelper2(apNodeInterface.GetAddress(0), port);
  clientHelper2.SetAttribute("MaxPackets", UintegerValue(1000));
  clientHelper2.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  clientHelper2.SetAttribute("PacketSize", UintegerValue(payloadSize));

  ApplicationContainer clientApps2 = clientHelper2.Install(staNodes.Get(1));
  clientApps2.Start(Seconds(1.0));
  clientApps2.Stop(Seconds(10.0));

   UdpClientHelper clientHelper3(apNodeInterface.GetAddress(0), port);
  clientHelper3.SetAttribute("MaxPackets", UintegerValue(1000));
  clientHelper3.SetAttribute("Interval", TimeValue(Seconds(0.01)));
  clientHelper3.SetAttribute("PacketSize", UintegerValue(payloadSize));

  ApplicationContainer clientApps3 = clientHelper3.Install(staNodes.Get(2));
  clientApps3.Start(Seconds(1.0));
  clientApps3.Stop(Seconds(10.0));


  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  if (tracing == true) {
    wifiPhy.EnablePcapAll("wifi-simple");
  }

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  monitor->CheckForLostPackets();

  Ptr<Ipv4FlowClassifier> classifier =
      DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i =
           stats.begin();
       i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> "
              << t.destinationAddress << ")\n";
    std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
    std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
    std::cout << "  Throughput: "
              << i->second.rxBytes * 8.0 /
                     (i->second.timeLastRxPacket.GetSeconds() -
                      i->second.timeFirstTxPacket.GetSeconds()) /
                     1000000
              << " Mbps\n";
  }

  monitor->SerializeToXmlFile("wifi-simple.flowmon", true, true);

  Simulator::Destroy();

  return 0;
}