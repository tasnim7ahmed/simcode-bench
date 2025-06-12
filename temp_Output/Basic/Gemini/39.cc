#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"
#include <fstream>
#include <iostream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RateAdaptationSimulation");

int main(int argc, char* argv[]) {
    bool enableLogging = false;
    std::string distanceSteps = "5.0";
    std::string timeIntervals = "1.0";

    CommandLine cmd;
    cmd.AddValue("enableLogging", "Enable logging of rate changes", enableLogging);
    cmd.AddValue("distanceSteps", "Distance steps for STA movement (meters)", distanceSteps);
    cmd.AddValue("timeIntervals", "Time intervals for STA movement (seconds)", timeIntervals);
    cmd.Parse(argc, argv);

    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    NodeContainer apNode = NodeContainer(nodes.Get(0));
    NodeContainer staNode = NodeContainer(nodes.Get(1));

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(Seconds(0.1)));

    NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false),
                "HysteresisFactor", DoubleValue(1.0));

    wifi.SetRemoteStationManager("ns3::MinstrelHtWifiManager");
    NetDeviceContainer staDevice = wifi.Install(phy, mac, staNode);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterface = address.Assign(staDevice);

    UdpServerHelper server(9);
    ApplicationContainer serverApps = server.Install(staNode);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpClientHelper client(staInterface.GetAddress(0), 9);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295));
    client.SetAttribute("Interval", TimeValue(Time("0.000002s")));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(apNode);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

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

    Ptr<ConstantPositionMobilityModel> staMobility = CreateObject<ConstantPositionMobilityModel>();
    nodes.Get(1)->AggregateObject(staMobility);
    staMobility->SetPosition(Vector(1.0, 0.0, 0.0));
    mobility.Install(staNode);

    std::vector<double> distanceVec;
    std::vector<double> timeVec;

    std::stringstream ss1(distanceSteps);
    double i;
    while (ss1 >> i) {
        distanceVec.push_back(i);
        if (ss1.peek() == ',')
            ss1.ignore();
    }

    std::stringstream ss2(timeIntervals);
    double t;
    while (ss2 >> t) {
        timeVec.push_back(t);
        if (ss2.peek() == ',')
            ss2.ignore();
    }

    if (distanceVec.size() != timeVec.size()) {
        std::cerr << "Error: Number of distance steps and time intervals must be the same." << std::endl;
        return 1;
    }

    for (size_t j = 0; j < distanceVec.size(); ++j) {
        Simulator::Schedule(Seconds(timeVec[j]), [staMobility, distanceVec, j]() {
            staMobility->SetPosition(Vector(1.0 + distanceVec[j], 0.0, 0.0));
        });
    }

    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    Simulator::Stop(Seconds(10.0));

    std::string filename = "rate-adaptation-simulation.pcap";
    phy.EnablePcapAll(filename);

    if (enableLogging) {
        Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/Minstrel/TxRate",
                                      MakeCallback(&[](const Time& time, WifiMode mode) {
                                          std::cout << time.GetSeconds() << "s: Rate change: " << mode << std::endl;
                                      }));
    }

    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    std::ofstream out("throughput-vs-distance.dat");
    out << "# Distance (m)  Throughput (Mbps)" << std::endl;

    for (size_t j = 0; j < distanceVec.size(); ++j) {
        double throughputSum = 0.0;
        int flowCount = 0;
        for (auto it = stats.begin(); it != stats.end(); ++it) {
            Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(it->first);
            if (t.sourceAddress == apInterface.GetAddress(0) && t.destinationAddress == staInterface.GetAddress(0)) {
                double throughput = it->second.rxBytes * 8.0 / (it->second.timeLastRxPacket.GetSeconds() - it->second.timeFirstTxPacket.GetSeconds()) / 1000000;
                throughputSum += throughput;
                flowCount++;
            }
        }
        double averageThroughput = (flowCount > 0) ? throughputSum / flowCount : 0.0;
        out << 1.0 + distanceVec[j] << " " << averageThroughput << std::endl;
    }

    out.close();

    Gnuplot plot("throughput-vs-distance.png");
    plot.SetTitle("Throughput vs. Distance");
    plot.SetLegend("Distance (m)", "Throughput (Mbps)");
    plot.AddDataset("Throughput vs. Distance", "throughput-vs-distance.dat", Gnuplot::LINES);
    plot.GenerateOutput();

    Simulator::Destroy();
    return 0;
}