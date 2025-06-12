#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("TcpClient", LOG_LEVEL_INFO);
    LogComponentEnable("TcpServer", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    //
    // Star Topology (TCP)
    //
    NodeContainer tcpNodes;
    tcpNodes.Create(4); // 1 server, 3 clients

    PointToPointHelper p2pTcp;
    p2pTcp.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2pTcp.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer tcpDevices;
    for (int i = 1; i < 4; ++i) {
        NetDeviceContainer link = p2pTcp.Install(tcpNodes.Get(0), tcpNodes.Get(i));
        tcpDevices.Add(link.Get(0));
        tcpDevices.Add(link.Get(1));
    }

    // Install TCP stack
    InternetStackHelper stackTcp;
    stackTcp.Install(tcpNodes);

    Ipv4AddressHelper addressTcp;
    addressTcp.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer tcpInterfaces = addressTcp.Assign(tcpDevices);

    // TCP Server
    uint16_t portTcp = 50000;
    TcpEchoServerHelper echoServerTcp(portTcp);
    ApplicationContainer serverAppTcp = echoServerTcp.Install(tcpNodes.Get(0));
    serverAppTcp.Start(Seconds(1.0));
    serverAppTcp.Stop(Seconds(10.0));

    // TCP Clients
    TcpEchoClientHelper echoClientTcp(tcpInterfaces.GetAddress(1), portTcp);
    echoClientTcp.SetAttribute("MaxPackets", UintegerValue(100));
    echoClientTcp.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    echoClientTcp.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientAppsTcp;
    for (int i = 1; i < 4; ++i) {
        echoClientTcp.SetRemote(tcpInterfaces.GetAddress(i * 2 -1), portTcp);
        ApplicationContainer clientApp = echoClientTcp.Install(tcpNodes.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));
        clientAppsTcp.Add(clientApp);
    }

    //
    // Mesh Topology (UDP) - WiFi
    //
    NodeContainer udpNodes;
    udpNodes.Create(3);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
    Ssid ssid = Ssid("ns-3-ssid");
    mac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));

    NetDeviceContainer udpDevices = wifi.Install(phy, mac, udpNodes);

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                  "X", StringValue("0.0"),
                                  "Y", StringValue("0.0"),
                                  "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=10.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(udpNodes);

    // Install UDP stack
    InternetStackHelper stackUdp;
    stackUdp.Install(udpNodes);

    Ipv4AddressHelper addressUdp;
    addressUdp.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer udpInterfaces = addressUdp.Assign(udpDevices);

    // UDP Applications (each node sends to others)
    uint16_t portUdp = 50001;
    UdpEchoServerHelper echoServerUdp(portUdp);
    ApplicationContainer serverAppsUdp;

    for (uint32_t i = 0; i < udpNodes.GetN(); ++i) {
        ApplicationContainer serverApp = echoServerUdp.Install(udpNodes.Get(i));
        serverApp.Start(Seconds(3.0));
        serverApp.Stop(Seconds(10.0));
        serverAppsUdp.Add(serverApp);
    }


    UdpEchoClientHelper echoClientUdp(udpInterfaces.GetAddress(0), portUdp);
    echoClientUdp.SetAttribute("MaxPackets", UintegerValue(50));
    echoClientUdp.SetAttribute("Interval", TimeValue(Seconds(0.2)));
    echoClientUdp.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientAppsUdp;
    for(uint32_t i = 0; i < udpNodes.GetN(); ++i){
      for(uint32_t j = 0; j < udpNodes.GetN(); ++j){
        if(i != j){
          echoClientUdp.SetRemote(udpInterfaces.GetAddress(j), portUdp);
          ApplicationContainer clientApp = echoClientUdp.Install(udpNodes.Get(i));
          clientApp.Start(Seconds(4.0));
          clientApp.Stop(Seconds(10.0));
          clientAppsUdp.Add(clientApp);
        }
      }
    }

    // Flow Monitor
    FlowMonitorHelper flowMonitor;
    Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

    // Set up global routing (required for ad-hoc UDP)
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    //
    //  Animation
    //
    AnimationInterface anim("hybrid-network.xml");
    anim.SetConstantPosition(tcpNodes.Get(0), 0.0, 10.0);
    anim.SetConstantPosition(tcpNodes.Get(1), 10.0, 0.0);
    anim.SetConstantPosition(tcpNodes.Get(2), 10.0, 10.0);
    anim.SetConstantPosition(tcpNodes.Get(3), 10.0, 20.0);

    // Run Simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Flow Monitor Analysis
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowMonitor.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << " Src Port " << t.sourcePort << " Dst Port " << t.destinationPort << "\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " kbps\n";
    }

    monitor->SerializeToXmlFile("hybrid-network.flowmon", true, true);

    Simulator::Destroy();
    return 0;
}