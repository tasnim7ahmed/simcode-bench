#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/stats-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiDistanceExperiment");

void TransmitCallback(Ptr<Packet const> packet, Mac48Address to) {
    DataCollector::Get()->AddData("Packets Transmitted", 1);
}

void ReceiveCallback(Mac48Address from, Ptr<Packet const> packet) {
    DataCollector::Get()->AddData("Packets Received", 1);
}

int main(int argc, char *argv[]) {
    std::string format = "both";
    double distance = 50.0;
    uint32_t runId = 0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("format", "Output format (none|omnet|sqlite|both)", format);
    cmd.AddValue("distance", "Distance between nodes in meters", distance);
    cmd.AddValue("runId", "Run identifier for experiment data", runId);
    cmd.Parse(argc, argv);

    // Set up logging
    LogComponentEnable("WifiDistanceExperiment", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Setup Wi-Fi in Ad-hoc mode
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiPhyHelper phy;
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Setup mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup applications: UDP Echo Client and Server
    uint16_t port = 9;
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApp = server.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpEchoClientHelper client(interfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(20));
    client.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Setup callbacks for packet tracking
    Config::Connect("/NodeList/0/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTx",
                    MakeCallback(&TransmitCallback));
    Config::Connect("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRx",
                    MakeCallback(&ReceiveCallback));

    // Setup DataCollector
    DataCollector collector;
    collector.Start(Time::Now());
    collector.Stop(Seconds(10.0));

    collector.AddMetadata("experiment-type", "wifi-distance-performance");
    collector.AddMetadata("distance", std::to_string(distance));
    collector.AddMetadata("run-id", std::to_string(runId));

    if (format == "omnet" || format == "both") {
        OmnetDataOutput omnetOutput;
        omnetOutput.Output(collector);
    }

    if (format == "sqlite" || format == "both") {
        SqliteDataOutput sqliteOutput;
        sqliteOutput.SetFilePrefix("wifi_distance_experiment");
        sqliteOutput.Output(collector);
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}