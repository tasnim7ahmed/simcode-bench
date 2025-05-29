#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiRateComparison");

int main(int argc, char *argv[]) {
    bool verbose = false;
    std::string rateControl = "ParfWifiManager";
    uint32_t rtsThreshold = 2347;
    std::string throughputFilename = "throughput.dat";
    std::string powerFilename = "power.dat";

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log if enabled.", verbose);
    cmd.AddValue("rateControl", "Rate control algorithm [ParfWifiManager|AparfWifiManager|RrpaaWifiManager]", rateControl);
    cmd.AddValue("rtsThreshold", "RTS/CTS threshold", rtsThreshold);
    cmd.AddValue("throughputFilename", "Throughput output filename", throughputFilename);
    cmd.AddValue("powerFilename", "Transmit power output filename", powerFilename);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNode;
    staNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n);
    wifi.SetRemoteStationManager(rateControl, "RtsCtsThreshold", UintegerValue(rtsThreshold));

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevice = wifi.Install(phy, mac, staNode);

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

    mobility.SetMobilityModel("ns3::GaussMarkovMobilityModel",
                              "Bounds", BoxValue(Box(0, 0, 100, 100, 100)),
                              "SigmaVelocity", DoubleValue(20),
                              "MeanVelocity", DoubleValue(0),
                              "SigmaDistance", DoubleValue(20));
    mobility.Install(staNode);

    Ptr<MobilityModel> staMobility = staNode.Get(0)->GetObject<MobilityModel>();

    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterface = address.Assign(staDevice);

    UdpServerHelper server(9);
    ApplicationContainer serverApp = server.Install(staNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpClientHelper client(staInterface.GetAddress(0), 9);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295));
    client.SetAttribute("Interval", TimeValue(Seconds(0.000148148)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = client.Install(apNode.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    // Define traces
    AsciiTraceHelper ascii;
    phy.EnableAsciiAll(ascii.CreateFileStream("wifi-simple-adhoc-grid.tr"));
    phy.EnablePcapAll("wifi-simple-adhoc-grid");

    // Add callback for logging transmit power and distance
    std::ofstream powerStream(powerFilename);
    powerStream << "Time\tDistance\tTxPower" << std::endl;

    Simulator::Schedule(Seconds(0.1), [&]() {
        Simulator::Schedule(Seconds(1), [&, staMobility, powerStream]() mutable {
            Vector pos = staMobility->GetPosition();
            double distance = std::sqrt(pos.x * pos.x + pos.y * pos.y);
            double txPower = phy.GetTransmitPowerLevel();
            powerStream << Simulator::Now().GetSeconds() << "\t" << distance << "\t" << txPower << std::endl;
            staMobility->SetPosition(Vector(pos.x + 1, pos.y, pos.z));

             if (Simulator::Now().GetSeconds() >= 10.0) {
                powerStream.close();
                return;
            }
        Simulator::Schedule(Seconds(1), std::bind(Simulator::Schedule, Seconds(1), std::ref(*this), staMobility, std::ref(powerStream)));
        });
    });

    Simulator::Stop(Seconds(10.0));

    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    std::ofstream throughputStream(throughputFilename);
    throughputStream << "Time\tThroughput" << std::endl;

    for (auto const& [flowId, flowStats] : stats) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flowId);
        if (t.sourceAddress == apInterface.GetAddress(0) && t.destinationAddress == staInterface.GetAddress(0)) {
            double throughput = flowStats.rxBytes * 8.0 / (flowStats.timeLastRxPacket.GetSeconds() - flowStats.timeFirstTxPacket.GetSeconds()) / 1000000;
            throughputStream << Simulator::Now().GetSeconds() << "\t" << throughput << std::endl;
            break; // Assuming only one flow of interest
        }
    }
    throughputStream.close();

    monitor->SerializeToXmlFile("wifi_flowmon.xml", true, true);
    Simulator::Destroy();
    return 0;
}