#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTosSimulation");

int main(int argc, char *argv[]) {
    bool verbose = false;
    uint32_t nWifi = 3;
    double distance = 1.0;
    std::string mcs = "0";
    std::string channelWidth = "20";
    std::string guardInterval = "Long";
    bool rtsCts = false;

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log if possible", verbose);
    cmd.AddValue("numStations", "Number of wifi stations", nWifi);
    cmd.AddValue("distance", "Distance between AP and stations", distance);
    cmd.AddValue("mcs", "HT MCS value (0-7)", mcs);
    cmd.AddValue("channelWidth", "Channel Width (20 or 40)", channelWidth);
    cmd.AddValue("guardInterval", "Guard Interval (Long or Short)", guardInterval);
    cmd.AddValue("rtsCts", "Enable RTS/CTS", rtsCts);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    NodeContainer staNodes;
    staNodes.Create(nWifi);

    NodeContainer apNode;
    apNode.Create(1);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("ns-3-ssid");

    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, staNodes);

    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid),
                    "BeaconGeneration", BooleanValue(true));

    NetDeviceContainer apDevices = wifi.Install(wifiPhy, wifiMac, apNode);

    MobilityHelper mobility;

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(distance),
                                  "GridWidth", UintegerValue(nWifi),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(staNodes);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);

    Ptr<ConstantPositionMobilityModel> apMobility = apNode.Get(0)->GetObject<ConstantPositionMobilityModel>();
    apMobility->SetPosition(Vector(0.0, 0.0, 0.0));

    InternetStackHelper internet;
    internet.Install(apNode);
    internet.Install(staNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i = ipv4.Assign(NetDeviceContainer::Create(apDevices, staDevices));

    UdpServerHelper server(9);
    ApplicationContainer serverApps = server.Install(apNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpClientHelper client(i.GetAddress(0), 9);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295));
    client.SetAttribute("Interval", TimeValue(Time("0.00001")));
    client.SetAttribute("PacketSize", UintegerValue(1472));

    ApplicationContainer clientApps = client.Install(staNodes);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", StringValue (channelWidth));
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/GuardInterval", StringValue (guardInterval));
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ShortGuardEnabled", BooleanValue (guardInterval == "Short"));

    if (rtsCts) {
        Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/RtsCtsThreshold", UintegerValue (1));
    }

    for (uint32_t j = 0; j < apDevices.GetN (); ++j)
      {
        Ptr<WifiNetDevice> device = DynamicCast<WifiNetDevice> (apDevices.Get (j));
        Ptr<HtWifiMac> mac = DynamicCast<HtWifiMac> (device->GetMac ());
        mac->SetMcsForId (0, mcs);
      }

    for (uint32_t j = 0; j < staDevices.GetN (); ++j)
      {
        Ptr<WifiNetDevice> device = DynamicCast<WifiNetDevice> (staDevices.Get (j));
        Ptr<HtWifiMac> mac = DynamicCast<HtWifiMac> (device->GetMac ());
        mac->SetMcsForId (0, mcs);
      }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(10.0));

    Simulator::Run();

    monitor->CheckForLostPackets ();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

    double totalThroughput = 0.0;
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
      {
          Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
          if (t.destinationPort == 9)
          {
              totalThroughput += i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
          }
      }

    std::cout << "Total Throughput: " << totalThroughput << " Mbps" << std::endl;

    Simulator::Destroy();
    return 0;
}