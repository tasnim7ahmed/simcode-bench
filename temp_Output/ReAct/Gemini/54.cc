#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  bool rtsCtsEnabled = false;
  std::string transportProtocol = "UDP";
  double distance = 10.0;

  CommandLine cmd;
  cmd.AddValue("rtsCtsEnabled", "Enable RTS/CTS (true/false)", rtsCtsEnabled);
  cmd.AddValue("transportProtocol", "Transport protocol (UDP/TCP)", transportProtocol);
  cmd.AddValue("distance", "Distance between STA and AP (meters)", distance);
  cmd.Parse(argc, argv);

  Time::SetResolution(Time::NS);

  NodeContainer nodes;
  nodes.Create(2);
  NodeContainer staNodes = NodeContainer(nodes.Get(0));
  NodeContainer apNodes = NodeContainer(nodes.Get(1));

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211ac);

  WifiMacHelper mac;

  Ssid ssid = Ssid("ns3-wifi");
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, staNodes);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "BeaconGeneration", BooleanValue(true),
              "BeaconInterval", TimeValue(Seconds(0.1)));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, apNodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(staNodes);
  mobility.Install(apNodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces;
  interfaces = address.Assign(staDevices);
  interfaces = address.Assign(apDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  uint16_t port = 5000;
  ApplicationContainer sourceApps, sinkApps;

  if (transportProtocol == "UDP") {
    UdpEchoServerHelper echoServer(port);
    sinkApps = echoServer.Install(apNodes.Get(0));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1000000));
    echoClient.SetAttribute("Interval", TimeValue(NanoSeconds(1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    sourceApps = echoClient.Install(staNodes.Get(0));
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(10.0));
  } else if (transportProtocol == "TCP") {
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    sinkApps = packetSinkHelper.Install(apNodes.Get(0));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0));
    sourceApps = bulkSendHelper.Install(staNodes.Get(0));
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(10.0));
  }

  for (int mcs = 0; mcs <= 9; ++mcs) {
    for (int channelWidth : {20, 40, 80, 160}) {
      for (bool shortGuardInterval : {true, false}) {
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue(channelWidth));
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/GuardInterval", BooleanValue(shortGuardInterval));
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ShortGuardEnabled", BooleanValue(shortGuardInterval));
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxMcs", IntegerValue(mcs));
        if (rtsCtsEnabled) {
          Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/RtsCtsThreshold", StringValue ("0"));
        } else {
           Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/RtsCtsThreshold", StringValue ("2200"));
        }
        Simulator::Stop(Seconds(11.0));

        FlowMonitorHelper flowMonitor;
        Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

        Simulator::Run();

        monitor->CheckForLostPackets();

        Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
        FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

        double goodput = 0.0;
        for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
          Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
          if (t.sourceAddress == interfaces.GetAddress(0) && t.destinationAddress == interfaces.GetAddress(1)) {
            goodput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
            break;
          }
        }

        std::cout << "MCS=" << mcs
                  << ", ChannelWidth=" << channelWidth
                  << ", ShortGI=" << shortGuardInterval
                  << ", RTS/CTS=" << rtsCtsEnabled
                  << ", Protocol=" << transportProtocol
                  << ", Goodput=" << goodput << " Mbps" << std::endl;

        Simulator::Destroy();
      }
    }
  }

  return 0;
}