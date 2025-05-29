#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/data-calculator.h"
#include "ns3/command-line.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiDistanceExperiment");

// DataCollector class to handle experiment data
class DataCollector {
public:
    DataCollector(std::string experimentName, std::string format, std::string run)
        : m_experimentName(experimentName),
          m_format(format),
          m_run(run)
    {}

    void Collect(double distance, double throughput) {
        if (m_format == "omnet") {
            std::cout << "omnet: "
                      << "experiment=" << m_experimentName << " "
                      << "distance=" << distance << " "
                      << "throughput=" << throughput << " "
                      << "run=" << m_run
                      << std::endl;
        } else if (m_format == "sqlite") {
            // In real application, implement SQLite database interaction here
            std::cout << "sqlite: Would store data in SQLite database." << std::endl;
        } else {
            std::cerr << "Error: Unknown output format: " << m_format << std::endl;
        }
    }

private:
    std::string m_experimentName;
    std::string m_format;
    std::string m_run;
};


int main(int argc, char* argv[]) {
    // Enable logging
    LogComponentEnable("WifiDistanceExperiment", LOG_LEVEL_INFO);

    // Command-line arguments
    double distance = 10.0; // Default distance
    std::string format = "omnet"; // Default output format
    std::string run = "1"; // Default run ID
    std::string experimentName = "WifiDistanceTest";

    CommandLine cmd;
    cmd.AddValue("distance", "Distance between nodes (meters)", distance);
    cmd.AddValue("format", "Output format (omnet or sqlite)", format);
    cmd.AddValue("run", "Run identifier", run);
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure Wi-Fi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Configure mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(1),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Create application:  UDP echo server on node 1
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Create application:  UDP echo client on node 0
    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Define a packet sink to measure throughput
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(serverApps.Get(0));

    // Create DataCollector
    DataCollector dataCollector(experimentName, format, run);


    // Run the simulation
    Simulator::Stop(Seconds(11.0));
    Simulator::Run();

    // Calculate throughput
    double throughput = 0.0;
    if (sink) {
        throughput = sink->GetTotalRx() * 8.0 / (10.0 - 2.0) / 1000000; // Mbps
        NS_LOG_INFO("Throughput: " << throughput << " Mbps");
    } else {
        NS_LOG_ERROR("PacketSink not found!");
    }

    // Collect data
    dataCollector.Collect(distance, throughput);

    monitor->CheckForLostPackets();
    std::ofstream os;
    os.open("distance.flowmon", std::ios::out);
    monitor->SerializeToXmlFile("distance.flowmon", true, true);
    os.close();

    Simulator::Destroy();
    return 0;
}