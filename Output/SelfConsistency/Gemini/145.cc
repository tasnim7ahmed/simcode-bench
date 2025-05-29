#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mesh-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMeshExample");

int main(int argc, char *argv[]) {
    bool enablePcap = true;
    double simulationTime = 10.0;

    CommandLine cmd;
    cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    NodeContainer meshNodes;
    meshNodes.Create(3);

    // Set up Wi-Fi mesh network
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211s);

    MeshHelper mesh;
    mesh.SetStackInstaller("ns3::Dot11sStack");
    mesh.SetMacType("RandomStart");

    NetDeviceContainer meshDevices = mesh.Install(wifi, meshNodes);

    // Set up mobility for static positioning
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(meshNodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(meshNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(meshDevices);

    // Create UDP traffic: node 0 sends to node 2
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(meshNodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(2), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
    echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(meshNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime));


    // Enable PCAP tracing
    if (enablePcap) {
        wifi.EnablePcapAll("mesh-example");
    }

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Animation Interface
    AnimationInterface anim("mesh-animation.xml");
    anim.SetConstantPosition(meshNodes.Get(0), 0.0, 0.0);
    anim.SetConstantPosition(meshNodes.Get(1), 5.0, 0.0);
    anim.SetConstantPosition(meshNodes.Get(2), 10.0, 0.0);

    // Run the simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
      std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Lost Packets:   " << i->second.lostPackets << "\n";
    }


    Simulator::Destroy();

    return 0;
}