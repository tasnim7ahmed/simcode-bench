#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologyExample");

int main(int argc, char *argv[])
{
    // Set logging
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Simulation parameters
    uint32_t nSpokes = 8;
    double simulationTime = 10.0; // seconds

    // Command line arguments (optional)
    CommandLine cmd;
    cmd.AddValue("nSpokes", "Number of spokes", nSpokes);
    cmd.Parse(argc, argv);

    // Create node containers
    NodeContainer hubNode;
    hubNode.Create(1);

    NodeContainer spokeNodes;
    spokeNodes.Create(nSpokes);

    // Create star topology (using n point-to-point links)
    std::vector<NodeContainer> nodePairs(nSpokes);
    std::vector<NetDeviceContainer> devices(nSpokes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    for (uint32_t i = 0; i < nSpokes; ++i)
    {
        nodePairs[i] = NodeContainer(hubNode.Get(0), spokeNodes.Get(i));
        devices[i] = p2p.Install(nodePairs[i]);
    }

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(hubNode);
    internet.Install(spokeNodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    std::vector<Ipv4InterfaceContainer> interfaces(nSpokes);
    for (uint32_t i = 0; i < nSpokes; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        ipv4.SetBase(Ipv4Address(subnet.str().c_str()), "255.255.255.0");
        interfaces[i] = ipv4.Assign(devices[i]);
    }

    // Enable global static routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create packet sink on hub to receive TCP traffic
    uint16_t sinkPort = 50000;
    Address sinkAddress (InetSocketAddress(interfaces[0].GetAddress(0), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(hubNode.Get(0));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simulationTime));

    // Configure OnOff apps for each spoke
    for (uint32_t i = 0; i < nSpokes; ++i)
    {
        // The hub's interface address on this link is interfaces[i].GetAddress(0)
        AddressValue remoteAddress (InetSocketAddress(interfaces[i].GetAddress(0), sinkPort));
        OnOffHelper onoff("ns3::TcpSocketFactory", Address());
        onoff.SetAttribute("Remote", remoteAddress);
        onoff.SetAttribute("PacketSize", UintegerValue(137));
        onoff.SetAttribute("DataRate", StringValue("14kbps"));
        // Optional: App start/stop times
        ApplicationContainer app = onoff.Install(spokeNodes.Get(i));
        app.Start(Seconds(1.0));
        app.Stop(Seconds(simulationTime));
    }

    // Enable pcap tracing for all point-to-point devices
    for (uint32_t i = 0; i < nSpokes; ++i)
    {
        std::ostringstream ss;
        ss << "star-p2p-" << i;
        p2p.EnablePcapAll(ss.str(), false);
    }

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}