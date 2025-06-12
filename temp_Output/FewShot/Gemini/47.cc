#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SpatialReuseSimulation");

int main(int argc, char *argv[]) {
    bool enableObssPd = true;
    double d1 = 5.0;
    double d2 = 10.0;
    double d3 = 15.0;
    double txPower = 20.0;
    int ccaEdThreshold = -82;
    int obssPdThreshold = -75;
    std::string dataRate = "OfdmRate6Mbps";
    std::string channelWidth = "20MHz";
    double simulationTime = 10.0;
    double sta1Start = 1.0;
    double sta2Start = 1.0;
    double sta1Stop = simulationTime;
    double sta2Stop = simulationTime;
    double sta1Interval = 0.01;
    double sta2Interval = 0.01;
    uint32_t packetSize = 1024;
    std::string logFile = "spatial-reuse.txt";

    CommandLine cmd;
    cmd.AddValue("enableObssPd", "Enable OBSS-PD Spatial Reuse (true/false)", enableObssPd);
    cmd.AddValue("d1", "Distance between STA1 and AP1 (meters)", d1);
    cmd.AddValue("d2", "Distance between AP1 and AP2 (meters)", d2);
    cmd.AddValue("d3", "Distance between STA2 and AP2 (meters)", d3);
    cmd.AddValue("txPower", "Transmit Power (dBm)", txPower);
    cmd.AddValue("ccaEdThreshold", "CCA ED Threshold (dBm)", ccaEdThreshold);
    cmd.AddValue("obssPdThreshold", "OBSS PD Threshold (dBm)", obssPdThreshold);
    cmd.AddValue("dataRate", "Data Rate", dataRate);
    cmd.AddValue("channelWidth", "Channel Width (20MHz, 40MHz, 80MHz)", channelWidth);
    cmd.AddValue("simulationTime", "Simulation Time (seconds)", simulationTime);
    cmd.AddValue("sta1Start", "STA1 start time", sta1Start);
    cmd.AddValue("sta1Stop", "STA1 stop time", sta1Stop);
    cmd.AddValue("sta2Start", "STA2 start time", sta2Start);
    cmd.AddValue("sta2Stop", "STA2 stop time", sta2Stop);
    cmd.AddValue("sta1Interval", "STA1 interval", sta1Interval);
    cmd.AddValue("sta2Interval", "STA2 interval", sta2Interval);
    cmd.AddValue("packetSize", "Packet Size", packetSize);
    cmd.AddValue("logFile", "Log file name", logFile);
    cmd.Parse(argc, argv);

    LogComponentEnable("SpatialReuseSimulation", LOG_LEVEL_INFO);

    NodeContainer staNodes;
    staNodes.Create(2);
    NodeContainer apNodes;
    apNodes.Create(2);

    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211ax);

    YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default();
    phyHelper.SetChannel(channelHelper.Create());

    Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue("2200"));
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("2200"));

    WifiMacHelper macHelper;

    Ssid ssid1 = Ssid("BSS1");
    Ssid ssid2 = Ssid("BSS2");

    NetDeviceContainer staDevices1, staDevices2, apDevices1, apDevices2;

    // BSS1
    macHelper.SetType("ns3::StaWifiMac",
                      "Ssid", SsidValue(ssid1),
                      "ActiveProbing", BooleanValue(false));
    staDevices1 = wifiHelper.Install(phyHelper, macHelper, staNodes.Get(0));

    macHelper.SetType("ns3::ApWifiMac",
                      "Ssid", SsidValue(ssid1),
                      "BeaconInterval", TimeValue(MilliSeconds(100)));
    apDevices1 = wifiHelper.Install(phyHelper, macHelper, apNodes.Get(0));

    // BSS2
    macHelper.SetType("ns3::StaWifiMac",
                      "Ssid", SsidValue(ssid2),
                      "ActiveProbing", BooleanValue(false));
    staDevices2 = wifiHelper.Install(phyHelper, macHelper, staNodes.Get(1));

    macHelper.SetType("ns3::ApWifiMac",
                      "Ssid", SsidValue(ssid2),
                      "BeaconInterval", TimeValue(MilliSeconds(100)));
    apDevices2 = wifiHelper.Install(phyHelper, macHelper, apNodes.Get(1));

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);

    mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=20.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=20.0]"),
                                  "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=5.0]"));

    mobility.Install(staNodes);

    Ptr<ConstantPositionMobilityModel> sta1Mobility = staNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
    sta1Mobility->SetPosition(Vector(0.0, 0.0, 0.0));

    Ptr<ConstantPositionMobilityModel> sta2Mobility = staNodes.Get(1)->GetObject<ConstantPositionMobilityModel>();
    sta2Mobility->SetPosition(Vector(d2, d3, 0.0));

    Ptr<ConstantPositionMobilityModel> ap1Mobility = apNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
    ap1Mobility->SetPosition(Vector(d1, 0.0, 0.0));

    Ptr<ConstantPositionMobilityModel> ap2Mobility = apNodes.Get(1)->GetObject<ConstantPositionMobilityModel>();
    ap2Mobility->SetPosition(Vector(d2, 0.0, 0.0));
    
    phyHelper.SetTxPowerStart(txPower);
    phyHelper.SetTxPowerEnd(txPower);

    phyHelper.SetCcaEdThreshold(ccaEdThreshold);
    phyHelper.SetObssPdThreshold(obssPdThreshold);

    if (!enableObssPd) {
        phyHelper.EnableObssPd(false);
    } else {
         phyHelper.EnableObssPd(true);
    }
  
    if (channelWidth == "40MHz") {
            Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (40));
    } else if (channelWidth == "80MHz") {
            Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (80));
    } else {
            Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (20));
    }

    InternetStackHelper internet;
    internet.Install(staNodes);
    internet.Install(apNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces1 = address.Assign(staDevices1);
    Ipv4InterfaceContainer apInterfaces1 = address.Assign(apDevices1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces2 = address.Assign(staDevices2);
    Ipv4InterfaceContainer apInterfaces2 = address.Assign(apDevices2);

    UdpClientHelper client1(apInterfaces1.GetAddress(0), 9);
    client1.SetAttribute("Interval", TimeValue(Seconds(sta1Interval)));
    client1.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApps1 = client1.Install(staNodes.Get(0));
    clientApps1.Start(Seconds(sta1Start));
    clientApps1.Stop(Seconds(sta1Stop));

    UdpServerHelper server1(9);
    ApplicationContainer serverApps1 = server1.Install(apNodes.Get(0));
    serverApps1.Start(Seconds(0.0));
    serverApps1.Stop(Seconds(simulationTime + 1));

    UdpClientHelper client2(apInterfaces2.GetAddress(0), 9);
    client2.SetAttribute("Interval", TimeValue(Seconds(sta2Interval)));
    client2.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApps2 = client2.Install(staNodes.Get(1));
    clientApps2.Start(Seconds(sta2Start));
    clientApps2.Stop(Seconds(sta2Stop));

    UdpServerHelper server2(9);
    ApplicationContainer serverApps2 = server2.Install(apNodes.Get(1));
    serverApps2.Start(Seconds(0.0));
    serverApps2.Stop(Seconds(simulationTime + 1));

    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    Simulator::Stop(Seconds(simulationTime + 1));

    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double throughput1 = 0.0;
    double throughput2 = 0.0;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        if (t.destinationAddress == apInterfaces1.GetAddress(0)) {
            throughput1 = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
            NS_LOG_INFO("BSS1 Throughput: " << throughput1 << " Mbps");
        }
        if (t.destinationAddress == apInterfaces2.GetAddress(0)) {
            throughput2 = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000;
            NS_LOG_INFO("BSS2 Throughput: " << throughput2 << " Mbps");
        }
    }

    monitor->SerializeToXmlFile("spatial-reuse.xml", true, true);

    Simulator::Destroy();

    std::ofstream outputFile(logFile);
    if (outputFile.is_open()) {
        outputFile << "BSS1 Throughput: " << throughput1 << " Mbps" << std::endl;
        outputFile << "BSS2 Throughput: " << throughput2 << " Mbps" << std::endl;
        outputFile.close();
    } else {
        std::cerr << "Unable to open log file: " << logFile << std::endl;
    }

    return 0;
}