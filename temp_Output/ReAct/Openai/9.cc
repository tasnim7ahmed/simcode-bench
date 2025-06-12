#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RingTopologyUdpEcho");

int main (int argc, char *argv[])
{
    LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create 4 nodes
    NodeContainer nodes;
    nodes.Create (4);

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

    // Arrange the ring: node 0-1, 1-2, 2-3, 3-0
    std::vector<NodeContainer> nodePairs;
    nodePairs.push_back (NodeContainer(nodes.Get(0), nodes.Get(1)));
    nodePairs.push_back (NodeContainer(nodes.Get(1), nodes.Get(2)));
    nodePairs.push_back (NodeContainer(nodes.Get(2), nodes.Get(3)));
    nodePairs.push_back (NodeContainer(nodes.Get(3), nodes.Get(0)));

    // Install NetDevices and keep track of them for addressing
    std::vector<NetDeviceContainer> netDevices;
    for (auto &pair : nodePairs)
    {
        netDevices.push_back (p2p.Install (pair));
    }

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install (nodes);

    // Assign IP addresses: one /30 subnet per link, but addressing in 192.9.39.0/24
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> interfaces;
    for (uint32_t i = 0; i < nodePairs.size(); ++i)
    {
        std::ostringstream subnet;
        subnet << "192.9.39." << (i * 4) << "/30";
        address.SetBase (subnet.str().c_str (), "255.255.255.252");
        interfaces.push_back (address.Assign (netDevices[i]));
    }

    // Set up UDP echo servers and clients
    uint16_t port = 9;
    double simTime = 15.0;
    double interval = 1.0;
    uint32_t maxPackets = 4;
    double appGap = 2.0;

    // Schedule: only one pair active at a time, rotating through the nodes
    // (client->server as 0->1, 1->2, 2->3, 3->0)
    ApplicationContainer serverApps, clientApps;

    for (uint32_t i = 0; i < 4; ++i)
    {
        uint32_t serverNodeIdx = (i + 1) % 4;
        uint32_t clientNodeIdx = i;

        // Server is installed on serverNode's loopback (choose first interface of incoming link)
        UdpEchoServerHelper echoServer (port);

        // Use PointToPoint link interface between client and server
        Ptr<Node> serverNode = nodes.Get(serverNodeIdx);
        Ipv4Address serverIp;
        // Node X->(X+1): Get the address of (X+1) from link X
        uint32_t ifaceIdx = (serverNodeIdx == 0) ? 3 : (serverNodeIdx - 1);
        serverIp = interfaces[ifaceIdx].GetAddress(1);

        ApplicationContainer serverApp = echoServer.Install (serverNode);
        serverApp.Start (Seconds (i * appGap));
        serverApp.Stop (Seconds (simTime));
        serverApps.Add (serverApp);

        UdpEchoClientHelper echoClient (serverIp, port);
        echoClient.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
        echoClient.SetAttribute ("Interval", TimeValue (Seconds (interval)));
        echoClient.SetAttribute ("PacketSize", UintegerValue (64));

        Ptr<Node> clientNode = nodes.Get(clientNodeIdx);
        ApplicationContainer clientApp = echoClient.Install (clientNode);
        clientApp.Start (Seconds (i * appGap + 0.5));
        clientApp.Stop (Seconds ((i * appGap) + (maxPackets * interval) + 1));
        clientApps.Add (clientApp);

        port++;
    }

    // Set up NetAnim positions for visualization
    AnimationInterface anim ("ring-topology.xml");
    anim.SetConstantPosition (nodes.Get(0), 50, 100);
    anim.SetConstantPosition (nodes.Get(1), 150, 150);
    anim.SetConstantPosition (nodes.Get(2), 250, 100);
    anim.SetConstantPosition (nodes.Get(3), 150, 50);

    Simulator::Stop (Seconds (simTime));
    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}