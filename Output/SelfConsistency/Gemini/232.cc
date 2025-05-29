#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpWifiExample");

int main(int argc, char *argv[]) {
    bool verbose = false;
    uint32_t nClients = 3;

    CommandLine cmd;
    cmd.AddValue("nClients", "Number of client nodes", nClients);
    cmd.AddValue("verbose", "Tell echo applications to log if true", verbose);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    NodeContainer clients;
    clients.Create(nClients);

    NodeContainer server;
    server.Create(1);

    NodeContainer allNodes;
    allNodes.Add(clients);
    allNodes.Add(server);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Create();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer clientDevices = wifi.Install(phy, mac, clients);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevices = wifi.Install(phy, mac, server);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(clients);

    mobility.SetPositionAllocator("ns3::ConstantPositionAllocator",
                                  "X", DoubleValue(20.0),
                                  "Y", DoubleValue(10.0));
    mobility.Install(server);

    InternetStackHelper internet;
    internet.Install(allNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer clientInterfaces = address.Assign(clientDevices);
    Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;  // Discard port (RFC 863)

    for (uint32_t i = 0; i < nClients; ++i) {
        UdpEchoClientHelper echoClient(apInterfaces.GetAddress(0), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(1));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));
        echoClient.Install(clients.Get(i));
    }

    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(server.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    for (auto i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
    }

    Simulator::Destroy();

    return 0;
}