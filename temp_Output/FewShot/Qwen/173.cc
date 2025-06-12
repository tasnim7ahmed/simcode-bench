#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set simulation parameters
    double simulationTime = 10.0;  // seconds

    // Enable logging for debugging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer wiredNodes;
    wiredNodes.Create(4);

    NodeContainer wirelessStaNodes;
    wirelessStaNodes.Create(2);

    NodeContainer wirelessApNode;
    wirelessApNode.Create(1);

    // Connect wired nodes in a chain using point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer wiredDevices[3];
    for (uint32_t i = 0; i < 3; ++i) {
        wiredDevices[i] = p2p.Install(NodeContainer(wiredNodes.Get(i), wiredNodes.Get(i + 1)));
    }

    // Install Internet stack on all wired nodes
    InternetStackHelper stack;
    stack.Install(wiredNodes);

    // Assign IP addresses to wired nodes
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer wiredInterfaces[3];
    for (uint32_t i = 0; i < 3; ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        wiredInterfaces[i] = address.Assign(wiredDevices[i]);
    }

    // Setup Wi-Fi network
    WifiMacHelper wifiMac;
    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_STANDARD_80211n_5GHz);
    wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"));

    Ssid ssid = Ssid("ns-3-ssid");

    // Configure access point
    wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifiHelper.Install(wirelessApNode.Get(0));

    // Configure stations
    wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifiHelper.Install(wirelessStaNodes);

    // Mobility model for wireless nodes
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(wirelessStaNodes);
    mobility.Install(wirelessApNode);

    // Install internet stack on wireless nodes
    stack.Install(wirelessApNode);
    stack.Install(wirelessStaNodes);

    // Assign IP address to AP and STAs
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // Connect wireless AP to one of the wired nodes (gateway)
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));
    NetDeviceContainer gatewayDevices = p2p.Install(wiredNodes.Get(2), wirelessApNode.Get(0));
    Ipv4InterfaceContainer gatewayInterfaces;
    address.SetBase("10.2.1.0", "255.255.255.0");
    gatewayInterfaces = address.Assign(gatewayDevices);

    // Enable global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup TCP traffic from wireless nodes to wired nodes
    uint16_t tcpPort = 5000;

    // Install sink application on wired node 0
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), tcpPort));
    ApplicationContainer sinkApps;
    for (uint32_t i = 0; i < wiredNodes.GetN(); ++i) {
        ApplicationContainer app = packetSinkHelper.Install(wiredNodes.Get(i));
        app.Start(Seconds(0.0));
        app.Stop(Seconds(simulationTime));
        sinkApps.Add(app);
    }

    // Install OnOff applications on wireless nodes
    OnOffHelper clientHelper("ns3::TcpSocketFactory", Address());
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < wirelessStaNodes.GetN(); ++i) {
        AddressValue remoteAddress(InetSocketAddress(wiredInterfaces[0].GetAddress(0), tcpPort));
        clientHelper.SetAttribute("Remote", remoteAddress);
        ApplicationContainer app = clientHelper.Install(wirelessStaNodes.Get(i));
        app.Start(Seconds(1.0));
        app.Stop(Seconds(simulationTime));
        clientApps.Add(app);
    }

    // Enable pcap tracing
    p2p.EnablePcapAll("tcp-wired-wireless-hybrid");

    // Setup FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Calculate throughput
    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    double totalThroughput = 0.0;
    uint32_t flowCount = 0;

    for (auto &[flowId, flowStats] : stats) {
        auto t = DynamicCast<TcpL4Protocol>(wiredNodes.Get(0)->GetObject<Protocol>());
        if (t != nullptr) {
            auto it = flowmon.GetClassifier()->FindFlow(flowId);
            if (it != flowmon.GetClassifier()->end()) {
                Ipv4FlowClassifier::FiveTuple tuple = it->second;
                if (tuple.destinationPort == tcpPort) {
                    totalThroughput += (flowStats.rxBytes * 8.0) / simulationTime / 1000 / 1000; // Mbps
                    flowCount++;
                }
            }
        }
    }

    if (flowCount > 0) {
        std::cout << "Average Throughput: " << totalThroughput / flowCount << " Mbps" << std::endl;
    } else {
        std::cout << "No flows recorded." << std::endl;
    }

    Simulator::Destroy();
    return 0;
}