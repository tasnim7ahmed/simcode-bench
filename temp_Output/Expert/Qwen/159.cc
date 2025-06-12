#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer starNodes;
    starNodes.Create(4); // Node 0 (central), Nodes 1-3 (peripheral)

    NodeContainer busNodes;
    busNodes.Create(3); // Nodes 4,5,6

    PointToPointHelper p2pStar;
    p2pStar.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pStar.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2pBus;
    p2pBus.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pBus.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer starDevices[3];
    for (uint32_t i = 0; i < 3; ++i) {
        starDevices[i] = p2pStar.Install(starNodes.Get(0), starNodes.Get(i + 1));
    }

    NetDeviceContainer busDevices[2];
    busDevices[0] = p2pBus.Install(busNodes.Get(0), busNodes.Get(1));
    busDevices[1] = p2pBus.Install(busNodes.Get(1), busNodes.Get(2));

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer starInterfaces[3];
    for (uint32_t i = 0; i < 3; ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        starInterfaces[i] = address.Assign(starDevices[i]);
    }

    Ipv4InterfaceContainer busInterfaces[2];
    for (uint32_t i = 0; i < 2; ++i) {
        std::ostringstream subnet;
        subnet << "10.2." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        busInterfaces[i] = address.Assign(busDevices[i]);
    }

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(starNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(20.0));

    UdpEchoClientHelper echoClient1(busInterfaces[0].GetAddress(0), 9);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(20));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp1 = echoClient1.Install(busNodes.Get(0));
    clientApp1.Start(Seconds(2.0));
    clientApp1.Stop(Seconds(20.0));

    UdpEchoClientHelper echoClient2(busInterfaces[1].GetAddress(0), 9);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(20));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.5)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp2 = echoClient2.Install(busNodes.Get(1));
    clientApp2.Start(Seconds(3.0));
    clientApp2.Stop(Seconds(20.0));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        FlowId id = iter->first;
        FlowMonitor::FlowStats s = iter->second;
        std::cout << "Flow ID: " << id << "  Packets Dropped: " << s.lostPackets << "  Delay: " << s.delaySum.GetSeconds() / s.rxPackets << "s" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}