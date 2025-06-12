#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("BranchTopologyUdpTest");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer centralNode;
    centralNode.Create(1);

    NodeContainer branchNodes;
    branchNodes.Create(3);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[3];
    Ipv4InterfaceContainer interfaces[3];

    InternetStackHelper stack;
    stack.Install(centralNode);
    stack.Install(branchNodes);

    for (uint8_t i = 0; i < 3; ++i) {
        devices[i] = p2p.Install(NodeContainer(centralNode.Get(0), branchNodes.Get(i)));
        std::ostringstream subnet;
        subnet << "10.1." << i << ".0";
        Ipv4AddressHelper address;
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces[i] = address.Assign(devices[i]);
    }

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(centralNode.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces[0].GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps[3];
    for (uint8_t i = 0; i < 3; ++i) {
        clientApps[i] = echoClient.Install(branchNodes.Get(i));
        clientApps[i].Start(Seconds(2.0));
        clientApps[i].Stop(Seconds(10.0));
    }

    p2p.EnablePcapAll("branch_topology_udp");

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}