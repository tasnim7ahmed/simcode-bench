#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetSimulation");

int main(int argc, char *argv[])
{
    uint32_t numNodes = 3;
    double simTime = 30.0;

    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Configure Wifi (ad hoc)
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomDirection2dMobilityModel",
                             "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)),
                             "Speed", StringValue("ns3::UniformRandomVariable[Min=2.0|Max=5.0]"),
                             "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"));
    mobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                 "X", StringValue("ns3::UniformRandomVariable[Min=10.0|Max=90.0]"),
                                 "Y", StringValue("ns3::UniformRandomVariable[Min=10.0|Max=90.0]"),
                                 "Z", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    mobility.Install(nodes);

    // Internet stack (with MANET routing)
    InternetStackHelper internet;
    OlsrHelper olsr;
    internet.SetRoutingHelper(olsr);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // UDP traffic: node 0 sends to node 2
    uint16_t port = 4000;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    UdpClientHelper client(interfaces.GetAddress(2), port);
    client.SetAttribute("MaxPackets", UintegerValue(10000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.2))); // 5 packets/sec
    client.SetAttribute("PacketSize", UintegerValue(64));
    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime - 1));

    // Tracing: track mobility and packet transmission
    AnimationInterface anim("manet.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));

    // Flow Monitor to calculate PDR and latency
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Calculate metrics
    double totTxPackets = 0;
    double totRxPackets = 0;
    double totDelaySum = 0;
    uint64_t totRx = 0;
    uint64_t totTx = 0;
    double avgDelay = 0.0;
    double pdr = 0.0;

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (auto& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        if (t.destinationPort == port)
        {
            totTx += flow.second.txPackets;
            totRx += flow.second.rxPackets;
            totDelaySum += flow.second.delaySum.GetSeconds();
        }
    }
    if (totRx > 0)
    {
        avgDelay = totDelaySum / totRx;
        pdr = (double)totRx / (double)totTx;
    }

    std::cout << "Packet Delivery Ratio (PDR): " << pdr * 100 << "%" << std::endl;
    std::cout << "Average end-to-end delay: " << avgDelay << " s" << std::endl;

    Simulator::Destroy();
    return 0;
}