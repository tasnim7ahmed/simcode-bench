#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t maxAmpduSize = 32;
    bool rtsCts = false;
    uint32_t payloadSize = 1472;
    double simulationTime = 10.0;
    double throughputMin = 0.0;
    double throughputMax = 200.0;

    CommandLine cmd;
    cmd.AddValue("rtsCts", "Enable/Disable RTS/CTS", rtsCts);
    cmd.AddValue("maxAmpduSize", "Number of aggregated MPDUs", maxAmpduSize);
    cmd.AddValue("payloadSize", "UDP payload size (bytes)", payloadSize);
    cmd.AddValue("simulationTime", "Simulation time (seconds)", simulationTime);
    cmd.AddValue("throughputMin", "Expected minimum throughput (Mbps)", throughputMin);
    cmd.AddValue("throughputMax", "Expected maximum throughput (Mbps)", throughputMax);
    cmd.Parse(argc, argv);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-80211n");

    /* MPDU Aggregation */
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue("HtMcs7"),
                                "ControlMode", StringValue("HtMcs0"));

    Config::SetDefault("ns3::HtWifiMac/MaxAmsduSize", UintegerValue(7935));
    Config::SetDefault("ns3::WifiRemoteStationManager/RtsCtsThreshold", UintegerValue(rtsCts ? 0 : 9999));
    Config::SetDefault("ns3::WifiMac/BE_MaxAmpduSize", UintegerValue(maxAmpduSize));

    NetDeviceContainer staDevices;
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    NetDeviceContainer apDevice;
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    apDevice = wifi.Install(phy, mac, wifiApNode);

    MobilityHelper mobility;

    // Place AP at (0,0,0)
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    // n1 at (0,50,0), n2 at (0,-50,0), so they can't hear each other (hidden node)
    positionAlloc->Add(Vector(0.0, 50.0, 0.0));
    positionAlloc->Add(Vector(0.0, -50.0, 0.0));

    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNodes);

    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);

    uint16_t port = 5000;
    ApplicationContainer serverApps;

    UdpServerHelper udpServer(port);
    serverApps = udpServer.Install(wifiApNode.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime + 1));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i)
    {
        UdpClientHelper udpClient(apInterface.GetAddress(0), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(0)); // unlimited
        udpClient.SetAttribute("Interval", TimeValue(MicroSeconds(100)));
        udpClient.SetAttribute("PacketSize", UintegerValue(payloadSize));
        clientApps.Add(udpClient.Install(wifiStaNodes.Get(i)));
    }
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime + 1));

    /* PCAP and ASCII traces */
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.EnablePcapAll("hidden-stations");
    AsciiTraceHelper ascii;
    phy.EnableAsciiAll(ascii.CreateFileStream("hidden-stations.tr"));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime + 1));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double sumThroughput = 0.0;
    for (auto const& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        if ((t.destinationAddress == apInterface.GetAddress(0)) && (t.destinationPort == port))
        {
            double throughput = (flow.second.rxBytes * 8.0) / (simulationTime * 1000000.0); // Mbps
            sumThroughput += throughput;
            std::cout << "Flow " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
            std::cout << "  Throughput: " << throughput << " Mbps\n";
        }
    }
    std::cout << "Total throughput: " << sumThroughput << " Mbps\n";
    if (sumThroughput < throughputMin || sumThroughput > throughputMax)
    {
        std::cout << "Throughput out of expected bounds (" << throughputMin << " - " << throughputMax << " Mbps)!" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}