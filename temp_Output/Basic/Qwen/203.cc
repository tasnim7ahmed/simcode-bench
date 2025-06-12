#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/animation-interface.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VanetSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes (vehicles)
    NodeContainer vehicles;
    vehicles.Create(5);

    // Setup mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(vehicles);

    // Assign initial speeds and directions
    for (uint32_t i = 0; i < vehicles.GetN(); ++i) {
        Ptr<Node> node = vehicles.Get(i);
        Ptr<MobilityModel> mob = node->GetObject<MobilityModel>();
        double speed = 10 + i; // Speeds from 10 to 14 m/s
        mob->SetAttribute("Velocity", VectorValue(Vector(speed, 0, 0)));
    }

    // Setup Wi-Fi with IEEE 802.11p
    WifiMacHelper wifiMac;
    WifiPhyHelper wifiPhy;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211_2016);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("OfdmRate6_5Mbps"),
                                 "ControlMode", StringValue("OfdmRate6_5Mbps"));

    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, vehicles);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(vehicles);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Set up UDP client-server application
    uint16_t port = 9;
    UdpServerHelper serverApp(port);
    ApplicationContainer serverApps = serverApp.Install(vehicles.Get(0)); // First vehicle is server
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpClientHelper client(interfaces.GetAddress(0), port); // Connect clients to server IP
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 1; i < vehicles.GetN(); ++i) {
        clientApps.Add(client.Install(vehicles.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    client.Stop(Seconds(10.0));

    // Enable flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Setup NetAnim visualization
    AnimationInterface anim("vanet_simulation.xml");
    anim.SetMobilityPollInterval(Seconds(0.1));

    // Simulation setup
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Output metrics
    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto iter : stats) {
        Ipv4FlowClassifier::FiveTuple t = static_cast<Ipv4FlowClassifier*>(flowmon.GetClassifier().Peek())->FindFlow(iter.first);
        std::cout << "Flow ID: " << iter.first << " Src Addr: " << t.sourceAddress << ":" << t.sourcePort
                  << " Dst Addr: " << t.destinationAddress << ":" << t.destinationPort << std::endl;
        std::cout << "  Tx Packets: " << iter.second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << iter.second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << iter.second.lostPackets << std::endl;
        std::cout << "  Packet Delivery Ratio: " << ((double)iter.second.rxPackets / (double)(iter.second.txPackets)) << std::endl;
        std::cout << "  Average Delay: " << iter.second.delaySum.GetSeconds() / iter.second.rxPackets << " s" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}