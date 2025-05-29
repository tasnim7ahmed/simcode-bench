#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetSimulation");

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Configure wireless channel
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Configure mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                  "X", StringValue("50.0"),
                                  "Y", StringValue("50.0"),
                                  "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=30.0]"));
    mobility.SetMobilityModel("ns3::RandomDirection2dMobilityModel",
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=10]"),
                              "Bounds", StringValue("50x50")); // Define bounds of the area
    mobility.Install(nodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Create UDP server and client applications
    UdpServerHelper server(9);
    ApplicationContainer serverApps = server.Install(nodes.Get(2)); // Server on node 2
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpClientHelper client(interfaces.GetAddress(2), 9);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(nodes.Get(0)); // Client on node 0
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    ApplicationContainer clientApps2 = client.Install(nodes.Get(1)); // Client on node 1
    clientApps2.Start(Seconds(2.0));
    clientApps2.Stop(Seconds(10.0));


    // Enable FlowMonitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Enable NetAnim
    AnimationInterface anim("manet-simulation.xml");
    anim.SetConstantPosition(nodes.Get(0), 10.0, 10.0);
    anim.SetConstantPosition(nodes.Get(1), 20.0, 20.0);
    anim.SetConstantPosition(nodes.Get(2), 30.0, 30.0);

    // Run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Gather statistics using FlowMonitor
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Packet Loss: " << i->second.lostPackets << "\n";
        std::cout << "  Packet Delivery Ratio: " << double(i->second.rxPackets) / double(i->second.txPackets) << "\n";
        std::cout << "  Delay Sum: " << i->second.delaySum << "\n";
        std::cout << "  Mean Delay: " << i->second.delaySum / i->second.rxPackets << "\n";

    }


    Simulator::Destroy();
    return 0;
}