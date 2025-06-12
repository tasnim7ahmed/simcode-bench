#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Logging (optional)
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    const uint32_t nSpokes = 8;
    NodeContainer hub;
    hub.Create(1);
    NodeContainer spokes;
    spokes.Create(nSpokes);

    NodeContainer starNodes;
    starNodes.Add(hub);
    starNodes.Add(spokes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    std::vector<NetDeviceContainer> devicePairs(nSpokes);
    std::vector<NodeContainer> pairNodes(nSpokes);

    for (uint32_t i = 0; i < nSpokes; ++i) {
        pairNodes[i] = NodeContainer(hub.Get(0), spokes.Get(i));
        devicePairs[i] = p2p.Install(pairNodes[i]);
        std::ostringstream oss;
        oss << "star-link-" << i;
        p2p.EnablePcapAll(oss.str(), false);
    }

    InternetStackHelper stack;
    stack.Install(hub);
    stack.Install(spokes);

    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> interfaces(nSpokes);

    for (uint32_t i = 0; i < nSpokes; ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces[i] = address.Assign(devicePairs[i]);
    }

    // Set up packet sink on hub for TCP port 50000
    uint16_t sinkPort = 50000;
    Address sinkAddress(InetSocketAddress(interfaces[0].GetAddress(0), sinkPort));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = sinkHelper.Install(hub.Get(0));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    // Set application parameters
    std::string packetSize = "137";
    std::string dataRate = "14kbps";

    // Each spoke sends TCP traffic to the hub
    for (uint32_t i = 0; i < nSpokes; ++i) {
        Address remoteAddr(InetSocketAddress(interfaces[i].GetAddress(0), sinkPort));

        OnOffHelper onoff("ns3::TcpSocketFactory", remoteAddr);
        onoff.SetAttribute("PacketSize", UintegerValue(137));
        onoff.SetAttribute("DataRate", StringValue(dataRate));
        onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
        onoff.SetAttribute("StopTime", TimeValue(Seconds(10.0)));
        ApplicationContainer app = onoff.Install(spokes.Get(i));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}