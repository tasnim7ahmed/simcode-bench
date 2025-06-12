#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>

using namespace ns3;

int main(int argc, char *argv[]) {
    bool verbose = false;
    uint32_t packetSize = 1472;
    std::string dataRate = "5Mbps";
    std::string phyRate = "MCS7";
    double simulationTime = 10;
    double distance = 10;
    double slotTime = 9e-6;
    double sifs = 16e-6;
    double pifs = 25e-6;

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue("packetSize", "size of packets generated", packetSize);
    cmd.AddValue("dataRate", "rate of packets generated", dataRate);
    cmd.AddValue("phyRate", "Physical layer rate", phyRate);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("distance", "Distance between nodes", distance);
    cmd.AddValue("slotTime", "Slot time in seconds", slotTime);
    cmd.AddValue("sifs", "SIFS time in seconds", sifs);
    cmd.AddValue("pifs", "PIFS time in seconds", pifs);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    NodeContainer apNodes;
    apNodes.Create(1);
    NodeContainer staNodes;
    staNodes.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);

    WifiMacHelper mac;

    Ssid ssid = Ssid("ns-3-ssid");
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevice = wifi.Install(phy, mac, apNodes);

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevice = wifi.Install(phy, mac, staNodes);

    MobilityHelper mobility;

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(apNodes);
    mobility.Install(staNodes);

    InternetStackHelper stack;
    stack.Install(apNodes);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterface = address.Assign(staDevice);

    UdpServerHelper server(9);
    ApplicationContainer serverApps = server.Install(staNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime + 1));

    UdpClientHelper client(staInterface.GetAddress(0), 9);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295));
    client.SetAttribute("Interval", TimeValue(DataRate(dataRate) / (packetSize * 8)));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = client.Install(apNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime + 1));

    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/Slot", TimeValue (Seconds (slotTime)));
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/Sifs", TimeValue (Seconds (sifs)));
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/Pifs", TimeValue (Seconds (pifs)));
    phy.Set("TxGain", DoubleValue(10));
    phy.Set("RxGain", DoubleValue(10));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime + 2));

    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double totalBytes = 0.0;
    Time firstTxTime;
    Time lastRxTime;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        if (t.sourceAddress == apInterface.GetAddress(0) && t.destinationAddress == staInterface.GetAddress(0)) {
            totalBytes = i->second.bytesReceived * 8.0;
            firstTxTime = i->second.timeFirstTxPacket;
            lastRxTime = i->second.timeLastRxPacket;
            break;
        }
    }

    double throughput = totalBytes / (lastRxTime.GetSeconds() - firstTxTime.GetSeconds()) / 1000000;

    std::cout << "Throughput: " << throughput << " Mbps" << std::endl;

    Simulator::Destroy();
    return 0;
}