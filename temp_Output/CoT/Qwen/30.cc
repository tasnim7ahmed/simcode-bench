#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiTosThroughputSimulation");

int main(int argc, char *argv[]) {
    uint32_t nStations = 5;
    uint8_t htMcs = 7;
    uint16_t channelWidth = 20;
    bool shortGuardInterval = false;
    double distance = 10.0; // meters
    bool enableRtsCts = false;
    double simulationTime = 10.0; // seconds

    CommandLine cmd(__FILE__);
    cmd.AddValue("nStations", "Number of stations", nStations);
    cmd.AddValue("htMcs", "HT MCS value (0-7)", htMcs);
    cmd.AddValue("channelWidth", "Channel width in MHz (20 or 40)", channelWidth);
    cmd.AddValue("shortGuardInterval", "Use short guard interval (true/false)", shortGuardInterval);
    cmd.AddValue("distance", "Distance between stations and AP (meters)", distance);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS handshake (true/false)", enableRtsCts);
    cmd.Parse(argc, argv);

    if (htMcs > 7 || (channelWidth != 20 && channelWidth != 40)) {
        NS_FATAL_ERROR("Invalid HT MCS or channel width");
    }

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nStations);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiPhyHelper phy;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("HtMcs" + std::to_string(htMcs)),
                                 "ControlMode", StringValue("HtMcs0"));

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");

    NetDeviceContainer staDevices;
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode.Get(0));

    Ptr<YansWifiChannel> channel = CreateObject<YansWifiChannel>();
    Ptr<LogDistancePropagationLossModel> loss = CreateObject<LogDistancePropagationLossModel>();
    loss->SetExponent(3.0);
    channel->AddPropagationLoss(loss);
    channel->SetPropagationDelayModel(CreateObject<ConstantSpeedPropagationDelayModel>());
    phy.SetChannel(channel);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0),
                                  "GridWidth", UintegerValue(nStations),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(5.0),
                                  "DeltaX", DoubleValue(0),
                                  "DeltaY", DoubleValue(0),
                                  "GridWidth", UintegerValue(1),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.Install(wifiApNode);

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    if (enableRtsCts) {
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/EnableRts",
                    BooleanValue(true));
    }

    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(wifiStaNodes);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    UdpClientHelper client;
    client.SetAttribute("RemotePort", UintegerValue(port));
    client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    client.SetAttribute("PacketSize", UintegerValue(1472)); // UDP payload size

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < wifiApNode.GetN(); ++i) {
        for (uint32_t j = 0; j < wifiStaNodes.GetN(); ++j) {
            client.SetAttribute("RemoteAddress", Ipv4AddressValue(staInterfaces.GetAddress(j)));
            clientApps.Add(client.Install(wifiApNode.Get(i)));
        }
    }
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    double totalRx = 0;
    for (auto & [flowId, flowStats] : stats) {
        auto srcAddr = monitor->GetClassifier()->FindFlow(flowId).sourceAddress;
        auto dstAddr = monitor->GetClassifier()->FindFlow(flowId).destinationAddress;
        if (serverApps.Get(0)->GetNode()->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal() == dstAddr) {
            totalRx += flowStats.rxBytes * 8.0 / simulationTime.GetSeconds();
        }
    }

    std::cout.precision(4);
    std::cout << "Aggregated UDP Throughput: " << (totalRx / 1e6) << " Mbps" << std::endl;

    Simulator::Destroy();
    return 0;
}