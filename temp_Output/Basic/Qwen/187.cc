#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/fat-tree-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FatTreeDataCenterSimulation");

int main(int argc, char *argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create Fat-Tree topology helper
    FatTreeHelper fatTree;

    // Build the Fat-Tree topology (k=4 for 4-port switches)
    Ptr<Topology> topology = fatTree.Build(4);

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(topology->GetAllNodes());

    // Assign IP addresses to all devices
    Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.0.0");
    topology->AssignIpv4Addresses(address);

    // Set up TCP connections between servers
    uint16_t port = 5000;
    for (uint32_t i = 0; i < topology->GetNumHosts(); ++i)
    {
        Ptr<Node> source = topology->GetHost(i);
        for (uint32_t j = 0; j < topology->GetNumHosts(); ++j)
        {
            if (i != j)
            {
                Ptr<Node> sink = topology->GetHost(j);
                Address sinkAddress(InetSocketAddress(topology->GetHostIpv4Address(j), port));

                // Install packet sink on the sink node
                PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory",
                                                  InetSocketAddress(Ipv4Address::GetAny(), port));
                ApplicationContainer sinkApp = packetSinkHelper.Install(sink);
                sinkApp.Start(Seconds(0.5));
                sinkApp.Stop(Seconds(10.0));

                // Install OnOff application on the source node
                OnOffHelper onoff("ns3::TcpSocketFactory", sinkAddress);
                onoff.SetConstantRate(DataRate("1Mbps"));
                onoff.SetAttribute("PacketSize", UintegerValue(1024));

                ApplicationContainer app = onoff.Install(source);
                app.Start(Seconds(1.0));
                app.Stop(Seconds(9.0));
            }
        }
    }

    // Set up PCAP tracing
    topology->EnablePcapAll("fat-tree-data-center");

    // Set up NetAnim visualization
    AnimationInterface anim("fat-tree-data-center.xml");
    anim.EnablePacketMetadata(true);

    // Run simulation
    Simulator::Stop(Seconds(10.5));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}