#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTimingParameters");

int main(int argc, char *argv[]) {
    bool verbose = false;
    uint32_t packetSize = 1472;
    std::string dataRate = "5Mbps";
    std::string phyMode = "OfdmRate6Mbps";
    double simulationTime = 10;
    double slotTime = 9e-6; // Default slot time
    double sifs = 16e-6;    // Default SIFS
    double pifs = 25e-6;    // Default PIFS

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log if set.", verbose);
    cmd.AddValue("packetSize", "Size of packets generated. Default: 1472", packetSize);
    cmd.AddValue("dataRate", "Data rate. Default: 5Mbps", dataRate);
    cmd.AddValue("phyMode", "Wifi Phy mode. Default: OfdmRate6Mbps", phyMode);
    cmd.AddValue("simulationTime", "Simulation time in seconds. Default: 10", simulationTime);
    cmd.AddValue("slotTime", "Slot time in seconds. Default: 9e-6", slotTime);
    cmd.AddValue("sifs", "SIFS in seconds. Default: 16e-6", sifs);
    cmd.AddValue("pifs", "PIFS in seconds. Default: 25e-6", pifs);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    NodeContainer apNode, staNode;
    apNode.Create(1);
    staNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconGeneration", BooleanValue(true),
                "BeaconInterval", TimeValue(Seconds(0.1)));

    NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevice = wifi.Install(phy, mac, staNode);

    // Set slot time, SIFS, and PIFS
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/Slot", TimeValue(Seconds(slotTime)));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/Sifs", TimeValue(Seconds(sifs)));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/Pifs", TimeValue(Seconds(pifs)));


    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    mobility.Install(staNode);

    InternetStackHelper internet;
    internet.Install(apNode);
    internet.Install(staNode);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i = ipv4.Assign(apDevice);
    ipv4.Assign(staDevice);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(staNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime + 1));

    UdpClientHelper client(i.GetAddress(1), port);
    client.SetPacketSize(packetSize);
    client.SetDataRate(dataRate);
    ApplicationContainer clientApps = client.Install(apNode.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime + 1));

    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double totalBytes = 0;
    for (auto i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        if (t.destinationAddress == i.GetAddress(1)) {
            totalBytes += i->second.bytesReceived;
        }
    }

    double throughput = totalBytes * 8 / (simulationTime * 1e6);
    std::cout << "Throughput: " << throughput << " Mbps" << std::endl;

    Simulator::Destroy();
    return 0;
}