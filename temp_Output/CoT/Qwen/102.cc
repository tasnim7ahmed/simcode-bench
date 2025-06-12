#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/multicast-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MulticastBlacklistRoutingSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(5); // A (0), B (1), C (2), D (3), E (4)

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[10]; // For up to 10 links
    int devIdx = 0;

    // Create a full mesh excluding blacklisted links
    std::map<std::pair<int, int>, bool> blacklist;
    // Example blacklist: disallow direct link from A to E
    blacklist[{0,4}] = true;
    blacklist[{4,0}] = true;

    for (int i = 0; i < 5; ++i) {
        for (int j = i + 1; j < 5; ++j) {
            if (blacklist.find({i,j}) == blacklist.end() && 
                blacklist.find({j,i}) == blacklist.end()) {
                NodeContainer pair = NodeContainer(nodes.Get(i), nodes.Get(j));
                devices[devIdx++] = p2p.Install(pair);
            }
        }
    }

    InternetStackHelper stack;
    Ipv4ListRoutingHelper listRH;
    Ipv4StaticRoutingHelper ipv4StaticRouting;

    listRH.Add(ipv4StaticRouting, 0);

    stack.SetRoutingHelper(listRH);
    stack.Install(nodes);

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[10];

    for (int i = 0; i < devIdx; ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << i << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces[i] = address.Assign(devices[i]);
    }

    // Multicast setup
    Ipv4Address multicastGroup("225.1.2.4");

    // Setup multicast routing on all nodes
    MulticastHelper multicast;
    multicast.Install(nodes);

    // Manually configure static unicast routes to control multicast path
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<Ipv4StaticRouting> ipv4StaticRouting = ipv4StaticRouting.GetStaticRouting(nodes.Get(i)->GetObject<Ipv4>());
        for (uint32_t j = 0; j < nodes.GetN(); ++j) {
            if (i != j) {
                Ipv4Address destAddr = nodes.Get(j)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
                Ipv4Address gateway;
                bool found = false;

                for (int d = 0; d < devIdx; ++d) {
                    for (uint32_t idx = 0; idx < interfaces[d].GetN(); ++idx) {
                        if (interfaces[d].GetAddress(idx).IsMatchingType(destAddr)) {
                            gateway = interfaces[d].GetAddress((idx+1)%2).GetLocal();
                            ipv4StaticRouting->AddHostRouteTo(destAddr, gateway, 1);
                            found = true;
                            break;
                        }
                    }
                    if (found) break;
                }

                if (!found) {
                    NS_FATAL_ERROR("No route found for node " << i << " to " << j);
                }
            }
        }
    }

    // Setup multicast sources and receivers
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps;

    for (int i = 1; i <= 4; ++i) { // B, C, D, E
        serverApps.Add(echoServer.Install(nodes.Get(i)));
    }
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(multicastGroup, 9)));
    onoff.SetConstantRate(DataRate("1Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = onoff.Install(nodes.Get(0)); // Node A
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Enable pcap tracing
    p2p.EnablePcapAll("multicast_blacklist_routing_simulation");

    Simulator::Run();
    
    // Print packet statistics
    uint32_t totalPackets = 0;
    for (int i = 0; i < 4; ++i) {
        Ptr<UdpEchoServer> server = DynamicCast<UdpEchoServer>(serverApps.Get(i));
        uint32_t packetsReceived = server->GetReceivedPacketCount();
        NS_LOG_INFO("Node " << i+1 << " received " << packetsReceived << " packets.");
        totalPackets += packetsReceived;
    }

    NS_LOG_INFO("Total packets received across all nodes: " << totalPackets);

    Simulator::Destroy();
    return 0;
}