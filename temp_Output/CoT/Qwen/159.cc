#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridTopologySimulation");

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
    address.SetBase("10.1.1.0", "255.255.255.0");
    for (uint32_t i = 0; i < 3; ++i) {
        starInterfaces[i] = address.Assign(starDevices[i]);
        address.NewNetwork();
    }

    Ipv4InterfaceContainer busInterfaces[2];
    address.SetBase("10.1.2.0", "255.255.255.0");
    for (uint32_t i = 0; i < 2; ++i) {
        busInterfaces[i] = address.Assign(busDevices[i]);
        address.NewNetwork();
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(starNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(20.0));

    UdpEchoClientHelper echoClient1(starNodes.Get(0)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), 9);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(20));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps1 = echoClient1.Install(busNodes.Get(0)); // Node 4
    clientApps1.Start(Seconds(2.0));
    clientApps1.Stop(Seconds(20.0));

    UdpEchoClientHelper echoClient2(starNodes.Get(0)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), 9);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(20));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps2 = echoClient2.Install(busNodes.Get(1)); // Node 5
    clientApps2.Start(Seconds(2.0));
    clientApps2.Stop(Seconds(20.0));

    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowMonitor = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    flowMonitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        FlowIdTag flowIdTag;
        bool found = false;
        for (auto jter = iter->second.begin(); jter != iter->second.end(); ++jter) {
            if (jter->second.txPackets > 0 && jter->second.rxPackets == 0) {
                found = true;
                break;
            }
        }
        if (found) {
            std::cout << "Flow ID: " << iter->first << " experienced packet loss." << std::endl;
        }
    }

    Simulator::Destroy();
    return 0;
}