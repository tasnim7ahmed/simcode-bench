#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    uint32_t numNodes = 2;
    double simTime = 20.0; // seconds
    uint32_t packetSize = 1024; // bytes
    uint32_t numPackets = 10;
    double interval = 1.0; // seconds

    NodeContainer nodes;
    nodes.Create(numNodes);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("DsssRate11Mbps"), "ControlMode", StringValue("DsssRate1Mbps"));

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)),
                             "Distance", DoubleValue(10.0),
                             "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(10.0),
                                 "MinY", DoubleValue(10.0),
                                 "DeltaX", DoubleValue(20.0),
                                 "DeltaY", DoubleValue(20.0),
                                 "GridWidth", UintegerValue(2),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(numPackets));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime));

    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();

    AnimationInterface anim("adhoc-two-nodes.xml");

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    flowmon->CheckForLostPackets();

    double totalTxPackets = 0;
    double totalRxPackets = 0;
    double totalLostPackets = 0;
    double totalDelay = 0;
    double totalThroughput = 0;
    uint32_t flowCount = 0;

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats();
    for (auto& f : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(f.first);
        if (t.destinationPort != echoPort)
            continue;

        totalTxPackets += f.second.txPackets;
        totalRxPackets += f.second.rxPackets;
        totalLostPackets += f.second.lostPackets;
        totalDelay += f.second.delaySum.GetSeconds();
        double rxKbits = f.second.rxBytes * 8.0 / 1024;
        double throughput = rxKbits / (simTime - 2.0); // Client starts at 2.0s
        totalThroughput += throughput;
        flowCount++;
    }

    double pdr = (totalTxPackets > 0) ? (totalRxPackets / totalTxPackets) * 100.0 : 0;
    double avgDelay = (totalRxPackets > 0) ? (totalDelay / totalRxPackets) : 0;
    double avgThroughput = (flowCount > 0) ? (totalThroughput / flowCount) : 0;

    std::cout << "========== Simulation Results ==========" << std::endl;
    std::cout << "Total transmitted packets: " << totalTxPackets << std::endl;
    std::cout << "Total received packets:    " << totalRxPackets << std::endl;
    std::cout << "Total lost packets:        " << totalLostPackets << std::endl;
    std::cout << "Packet Delivery Ratio:     " << pdr << " %" << std::endl;
    std::cout << "Average End-to-End Delay:  " << avgDelay << " s" << std::endl;
    std::cout << "Average Throughput:        " << avgThroughput << " kbps" << std::endl;
    std::cout << "========================================" << std::endl;

    Simulator::Destroy();
    return 0;
}