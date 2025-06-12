#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/config-store-module.h"
#include "ns3/command-line.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

  bool enablePcap = false;
  std::string phyMode = "SpectrumWifiPhy";
  double simulationTime = 10;
  double distance = 10;
  int mcsIndex = 7;
  bool useUdp = true;
  int packetSize = 1472;
  std::string rate ("50Mbps");

  CommandLine cmd;
  cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.AddValue("phyMode", "PHY mode (SpectrumWifiPhy or YansWifiPhy)", phyMode);
  cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue("distance", "Distance between nodes in meters", distance);
  cmd.AddValue("mcsIndex", "MCS index", mcsIndex);
  cmd.AddValue("useUdp", "Use UDP traffic (otherwise TCP)", useUdp);
  cmd.AddValue("packetSize", "Size of UDP packets", packetSize);
  cmd.AddValue("rate", "CBR traffic rate", rate);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::WifiMacQueue::MaxPackets", UintegerValue(2000));

  NodeContainer nodes;
  nodes.Create(2);

  WifiHelper wifiHelper;
  wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211n);

  YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default();
  phyHelper.SetChannel(channelHelper.Create());

  if (phyMode == "SpectrumWifiPhy") {
    phyHelper.Set("TxGain", DoubleValue(10));
  }
  else {
      phyHelper.Set("EnergyDetectionThreshold", DoubleValue (-101) );
      phyHelper.Set("CcaEdThreshold", DoubleValue (-98) );
  }

  WifiMacHelper macHelper;
  Ssid ssid = Ssid("ns-3-ssid");
  macHelper.SetType("ns3::StaWifiMac",
                     "Ssid", SsidValue(ssid),
                     "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifiHelper.Install(phyHelper, macHelper, nodes.Get(1));

  macHelper.SetType("ns3::ApWifiMac",
                     "Ssid", SsidValue(ssid));

  NetDeviceContainer apDevices;
  apDevices = wifiHelper.Install(phyHelper, macHelper, nodes.Get(0));

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

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(staDevices);
  address.Assign(apDevices);
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 9;

  ApplicationContainer app;
  if (useUdp) {
    UdpClientHelper client(interfaces.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295));
    client.SetAttribute("Interval", TimeValue(Seconds(0.00002)));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));
    app = client.Install(nodes.Get(1));

    UdpServerHelper server(port);
    app.Add(server.Install(nodes.Get(0)));
  } else {
    BulkSendHelper source("ns3::TcpSocketFactory",
                           InetSocketAddress(interfaces.GetAddress(0), port));
    source.SetAttribute("SendSize", UintegerValue(packetSize));
    app = source.Install(nodes.Get(1));

    PacketSinkHelper sink("ns3::TcpSocketFactory",
                             InetSocketAddress(Ipv4Address::GetAny(), port));
     app.Add(sink.Install(nodes.Get(0)));
  }
  app.Start(Seconds(1.0));
  app.Stop(Seconds(simulationTime - 1));


  if (enablePcap) {
    phyHelper.EnablePcap("wifi-simple", apDevices);
  }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(simulationTime));
  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
    std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
  }

  Simulator::Destroy();
  return 0;
}