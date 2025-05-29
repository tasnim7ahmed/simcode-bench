#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiRateControlComparison");

int main(int argc, char *argv[]) {
    bool verbose = false;
    std::string rateControl = "ParfWifiManager";
    uint32_t rtsThreshold = 2346;
    std::string throughputFilename = "throughput.dat";
    std::string txPowerFilename = "txpower.dat";
    double simulationTime = 20.0;

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log if possible", verbose);
    cmd.AddValue("rateControl", "Rate control algorithm to use", rateControl);
    cmd.AddValue("rtsThreshold", "RTS/CTS Threshold", rtsThreshold);
    cmd.AddValue("throughputFilename", "Throughput output filename", throughputFilename);
    cmd.AddValue("txPowerFilename", "Tx Power output filename", txPowerFilename);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(2);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Create();
    phy.SetChannel(channel.Create());

    wifi.SetRemoteStationManager(rateControl,
                                 "RtsCtsThreshold", UintegerValue(rtsThreshold));

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, nodes.Get(1));

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(Seconds(0.05)));

    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, mac, nodes.Get(0));

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes.Get(0));

    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes.Get(1));
    Ptr<ConstantVelocityMobilityModel> staMob = nodes.Get(1)->GetObject<ConstantVelocityMobilityModel>();
    staMob->SetVelocity(Vector3D(1, 0, 0));

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces;
    interfaces = address.Assign(staDevices);
    interfaces = address.Assign(apDevices);

    UdpServerHelper server(9);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime + 1));

    UdpClientHelper client(interfaces.GetAddress(1), 9);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295));
    client.SetAttribute("Interval", TimeValue(Time(0.000001))); // 1 us interval -> ~54 Mbps
    client.SetAttribute("PacketSize", UintegerValue(65507));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime + 1));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime + 1));

    AsciiTraceHelper ascii;
    phy.EnableAsciiAll(ascii.CreateFileStream("wifi-trace.tr"));
    phy.EnablePcapAll("wifi-trace");

    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    std::ofstream throughputFile(throughputFilename);
    std::ofstream txPowerFile(txPowerFilename);

    if (!throughputFile.is_open() || !txPowerFile.is_open()) {
        std::cerr << "Error opening output files." << std::endl;
        return 1;
    }

    throughputFile << "# Distance (m)  Throughput (Mbps)" << std::endl;
    txPowerFile << "# Distance (m)  Tx Power (dBm)" << std::endl;

    for (double t = 2.0; t <= simulationTime; t += 0.1) {
        Simulator::Schedule(Seconds(t), [&, t]() {
            double distance = t - 2.0;
            double throughput = 0.0;
            double txPowerSum = 0.0;
            int txPowerCount = 0;

            for (auto const& flowStat : stats) {
                Ipv4FlowClassifier::FiveTuple t5 = classifier->FindFlow(flowStat.first);
                if (t5.destinationAddress == interfaces.GetAddress(1)) {
                    throughput = flowStat.second.rxBytes * 8.0 / (flowStat.second.timeLastRxPacket.GetSeconds() - flowStat.second.timeFirstTxPacket.GetSeconds()) / 1000000;
                }
            }

            Ptr<WifiNetDevice> wifiNetDevice = DynamicCast<WifiNetDevice>(apDevices.Get(0));
            if (wifiNetDevice) {
                Ptr<YansWifiPhy> phyLayer = DynamicCast<YansWifiPhy>(wifiNetDevice->GetPhy());
                if (phyLayer) {
                  txPowerSum = phyLayer->GetTxPowerLevels().at(0);
                  txPowerCount = 1;
                }
            }


            throughputFile << distance << " " << throughput << std::endl;
            txPowerFile << distance << " " << txPowerSum << std::endl;

        });
    }

    Simulator::Destroy();
    throughputFile.close();
    txPowerFile.close();

    return 0;
}