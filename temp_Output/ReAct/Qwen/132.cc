#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-dv-routing-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TwoNodeLoopSimulation");

int main(int argc, char *argv[]) {
    bool enableFlowMonitor = false;
    uint32_t stopTime = 20;
    std::string pointToPointDataRate = "1Mbps";
    double pointToPointDelay = 2;

    CommandLine cmd(__FILE__);
    cmd.AddValue("stopTime", "Total simulation time (seconds)", stopTime);
    cmd.AddValue("enableFlowMonitor", "Enable Flow Monitor", enableFlowMonitor);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(pointToPointDataRate));
    p2p.SetChannelAttribute("Delay", TimeValue(Seconds(pointToPointDelay)));

    NetDeviceContainer devicesAB = p2p.Install(nodes.Get(0), nodes.Get(1));

    InternetStackHelper stack;
    DvRoutingHelper dvRouting;
    stack.SetRoutingHelper(dvRouting);
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfacesAB = address.Assign(devicesAB);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;

    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(stopTime));

    UdpEchoClientHelper echoClient(interfacesAB.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(stopTime - 1));

    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("two-node-loop.tr"));
    p2p.EnablePcapAll("two-node-loop");

    Simulator::Schedule(Seconds(5), &Ipv4InterfaceContainer::SetDown, &interfacesAB, 1);

    if (enableFlowMonitor) {
        FlowMonitorHelper flowmon;
        Ptr<FlowMonitor> monitor = flowmon.InstallAll();
        monitor->SetSummarizePerProtocol(true);
        Simulator::Stop(Seconds(stopTime));
        Simulator::Run();
        monitor->CheckForLostPackets();
        monitor->SerializeToXmlFile("two-node-loop.flowmon", true, true);
    } else {
        Simulator::Stop(Seconds(stopTime));
        Simulator::Run();
    }

    Simulator::Destroy();
    return 0;
}