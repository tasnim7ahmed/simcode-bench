#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessHandoverSimulation");

class HandoverStats {
public:
    uint32_t handovers = 0;
    uint64_t totalPacketsDropped = 0;

    void CountHandover() {
        handovers++;
    }

    void CountPacketDrop(Ptr<const Packet>) {
        totalPacketsDropped++;
    }
};

int main(int argc, char *argv[]) {
    uint32_t numNodes = 6;
    double simTime = 60.0;
    bool enableNetAnim = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("numNodes", "Number of mobile stations", numNodes);
    cmd.AddValue("simTime", "Total simulation time (seconds)", simTime);
    cmd.AddValue("enableNetAnim", "Enable NetAnim output", enableNetAnim);
    cmd.Parse(argc, argv);

    // Create APs and STAs
    NodeContainer apNodes;
    apNodes.Create(2);

    NodeContainer staNodes;
    staNodes.Create(numNodes);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;

    // Install first AP (SSID: network1)
    Ssid ssid1 = Ssid("network1");
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid1));
    NetDeviceContainer apDevices1 = wifi.Install(phy, mac, apNodes.Get(0));

    // Install second AP (SSID: network2)
    Ssid ssid2 = Ssid("network2");
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid2));
    NetDeviceContainer apDevices2 = wifi.Install(phy, mac, apNodes.Get(1));

    // Set up mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobility.Install(staNodes);

    // Install STA devices
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid1),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

    // Reassociate nodes to best AP based on RSS
    for (auto nodeIt = staNodes.Begin(); nodeIt != staNodes.End(); ++nodeIt) {
        Ptr<Node> node = *nodeIt;
        for (uint32_t i = 0; i < node->GetNDevices(); ++i) {
            Ptr<WifiNetDevice> device = DynamicCast<WifiNetDevice>(node->GetDevice(i));
            if (device) {
                device->GetMac()->TraceConnectWithoutContext("Assoc", MakeCallback(
                    [](Mac48Address address, uint16_t aid) {
                        NS_LOG_INFO("Node associated with AP: " << address);
                    }));
                device->GetMac()->TraceConnectWithoutContext("Deassoc", MakeCallback(
                    [](Mac48Address address) {
                        NS_LOG_INFO("Node deassociated from AP: " << address);
                    }));
            }
        }
    }

    // Internet stack
    InternetStackHelper stack;
    stack.Install(apNodes);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");

    Ipv4InterfaceContainer apInterfaces1 = address.Assign(apDevices1);
    address.NewNetwork();
    Ipv4InterfaceContainer apInterfaces2 = address.Assign(apDevices2);
    address.NewNetwork();
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // UDP Echo server setup
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(staNodes);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    // UDP Echo client setup
    UdpEchoClientHelper echoClient(staInterfaces.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10000));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = echoClient.Install(apNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime));

    // Connect packet drop trace
    HandoverStats stats;
    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/TxFailed", MakeCallback(&HandoverStats::CountPacketDrop, &stats));

    // Animation
    Ptr<NetAnimTrajectoryInterpolator> interpolator;
    if (enableNetAnim) {
        AnimationInterface anim("wireless-handover.xml");
        anim.EnablePacketMetadata(true);
        anim.EnableIpv4RouteTracking("routing-table-wireless-handover.txt", Seconds(0), Seconds(simTime), Seconds(0.25));
        interpolator = anim.GetTrajectoryInterpolator();
    }

    Simulator::Stop(Seconds(simTime));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Run();

    // Output statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> statsMap = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = statsMap.begin(); iter != statsMap.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
        std::cout << "Flow ID: " << iter->first << " Src Addr: " << t.sourceAddress << " Dst Addr: " << t.destinationAddress << std::endl;
        std::cout << "  Tx Packets: " << iter->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << iter->second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << iter->second.lostPackets << std::endl;
        std::cout << "  Packet delivery ratio: " << ((double)iter->second.rxPackets / (double)iter->second.txPackets) << std::endl;
    }

    std::cout << "Total Handovers: " << stats.handovers << std::endl;
    std::cout << "Total Packets Dropped: " << stats.totalPacketsDropped << std::endl;

    Simulator::Destroy();
    return 0;
}