#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologyUdpEcho");

int main(int argc, char *argv[]) {
    uint32_t nNodes = 5;
    double linkRate = 5e6;
    double linkDelay = 2e-3;
    uint32_t packetSize = 1024;
    uint32_t totalPacketsPerClient = 4;
    double interPacketInterval = 1.0;
    double simulationTime = (totalPacketsPerClient + nNodes) * interPacketInterval + 2;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nNodes", "Number of nodes in the star topology.", nNodes);
    cmd.AddValue("simulationTime", "Total simulation time (seconds)", simulationTime);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(nNodes);

    NodeContainer centralNode = nodes.Get(0);
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate(linkRate)));
    p2p.SetChannelAttribute("Delay", TimeValue(Seconds(linkDelay)));

    NetDeviceContainer devices[nNodes];
    Ipv4InterfaceContainer interfaces[nNodes];

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("192.9.39.0", "255.255.255.0");

    for (uint32_t i = 1; i < nNodes; ++i) {
        NodeContainer pair = NodeContainer(centralNode, nodes.Get(i));
        devices[i] = p2p.Install(pair);
        interfaces[i] = address.Assign(devices[i]);
        address.NewNetwork();
    }

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(centralNode);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    for (uint32_t i = 1; i < nNodes; ++i) {
        UdpEchoClientHelper echoClient(interfaces[i].GetAddress(0), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(totalPacketsPerClient));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(interPacketInterval)));
        echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

        ApplicationContainer clientApps = echoClient.Install(nodes.Get(i));
        clientApps.Start(Seconds(i * interPacketInterval));
        clientApps.Stop(Seconds(simulationTime));
    }

    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("star-topology-udp.tr"));
    p2p.EnablePcapAll("star-topology-udp");

    AnimationInterface anim("star-topology.xml");
    for (uint32_t i = 0; i < nNodes; ++i) {
        anim.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}