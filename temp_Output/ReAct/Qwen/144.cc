#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RingTopologySimulation");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 4;
    std::string dataRate = "5Mbps";
    std::string delay = "1ms";
    uint32_t packetSize = 1024;
    uint32_t interPacketIntervalMs = 1000;

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(dataRate));
    p2p.SetChannelAttribute("Delay", StringValue(delay));

    NetDeviceContainer devices[numNodes];
    for (uint32_t i = 0; i < numNodes; ++i) {
        uint32_t j = (i + 1) % numNodes;
        devices[i] = p2p.Install(nodes.Get(i), nodes.Get(j));
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[numNodes];
    for (uint32_t i = 0; i < numNodes; ++i) {
        std::ostringstream subnet;
        subnet << "10.0." << i << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces[i] = address.Assign(devices[i]);
    }

    uint16_t port = 9;
    for (uint32_t i = 0; i < numNodes; ++i) {
        uint32_t destNode = (i + 1) % numNodes;
        Ipv4Address destIp = interfaces[destNode].GetAddress(0);
        if (destNode == 0) {
            destIp = interfaces[0].GetAddress(1);
        } else {
            destIp = interfaces[destNode].GetAddress(0);
        }

        OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(destIp, port));
        onoff.SetConstantRate(DataRate("1kbps"));
        onoff.SetAttribute("PacketSize", UintegerValue(packetSize));

        ApplicationContainer apps = onoff.Install(nodes.Get(i));
        apps.Start(Seconds(0.0));
        apps.Stop(Seconds(10.0));
    }

    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("ring-topology-simulation.tr"));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}