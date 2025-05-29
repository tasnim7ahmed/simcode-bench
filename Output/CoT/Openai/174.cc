#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-ospf-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create all nodes
    NodeContainer ringNodes;
    ringNodes.Create(4);

    NodeContainer meshNodes;
    meshNodes.Create(4);

    NodeContainer busNodes;
    busNodes.Create(4);

    NodeContainer lineNodes;
    lineNodes.Create(4);

    NodeContainer starHub;
    starHub.Create(1);
    NodeContainer starSpokes;
    starSpokes.Create(4);

    NodeContainer treeLevel0;
    treeLevel0.Create(1);
    NodeContainer treeLevel1;
    treeLevel1.Create(2);
    NodeContainer treeLevel2;
    treeLevel2.Create(4);

    // Container for all nodes
    NodeContainer allNodes;
    allNodes.Add(ringNodes);
    allNodes.Add(meshNodes);
    allNodes.Add(busNodes);
    allNodes.Add(lineNodes);
    allNodes.Add(starHub);
    allNodes.Add(starSpokes);
    allNodes.Add(treeLevel0);
    allNodes.Add(treeLevel1);
    allNodes.Add(treeLevel2);

    // Point-to-Point Helper
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // 1. Ring topology
    NetDeviceContainer ringDevices[4];
    Ipv4InterfaceContainer ringIfaces[4];
    for (uint32_t i = 0; i < 4; ++i)
    {
        ringDevices[i] = p2p.Install(ringNodes.Get(i), ringNodes.Get((i + 1) % 4));
    }

    // 2. Mesh topology (fully connected mesh among meshNodes)
    NetDeviceContainer meshDevices[4][4];
    Ipv4InterfaceContainer meshIfaces[4][4];
    for (uint32_t i = 0; i < 4; ++i)
    {
        for (uint32_t j = i + 1; j < 4; ++j)
        {
            meshDevices[i][j] = p2p.Install(meshNodes.Get(i), meshNodes.Get(j));
        }
    }

    // 3. Bus topology (nodes connected to a virtual bus via chain, so node 0-1-2-3)
    NetDeviceContainer busDevices[3];
    for (uint32_t i = 0; i < 3; ++i)
    {
        busDevices[i] = p2p.Install(busNodes.Get(i), busNodes.Get(i + 1));
    }

    // 4. Line topology (linear chain)
    NetDeviceContainer lineDevices[3];
    for (uint32_t i = 0; i < 3; ++i)
    {
        lineDevices[i] = p2p.Install(lineNodes.Get(i), lineNodes.Get(i + 1));
    }

    // 5. Star topology (hub and spokes)
    NetDeviceContainer starDevices[4];
    for (uint32_t i = 0; i < 4; ++i)
    {
        starDevices[i] = p2p.Install(starHub.Get(0), starSpokes.Get(i));
    }

    // 6. Tree topology (1 root -> 2 -> 4 leaves)
    NetDeviceContainer treeDevicesL0L1[2];
    NetDeviceContainer treeDevicesL1L2[4];

    // Level 0 to 1
    for (uint32_t i = 0; i < 2; ++i)
    {
        treeDevicesL0L1[i] = p2p.Install(treeLevel0.Get(0), treeLevel1.Get(i));
    }
    // Level 1 to 2
    for (uint32_t i = 0; i < 2; ++i)
    {
        treeDevicesL1L2[i * 2 + 0] = p2p.Install(treeLevel1.Get(i), treeLevel2.Get(i * 2 + 0));
        treeDevicesL1L2[i * 2 + 1] = p2p.Install(treeLevel1.Get(i), treeLevel2.Get(i * 2 + 1));
    }

    // Interconnect topologies
    // Connect ring node 0 <-> mesh node 0
    NetDeviceContainer br1 = p2p.Install(ringNodes.Get(0), meshNodes.Get(0));
    // Connect mesh node 1 <-> bus node 0
    NetDeviceContainer br2 = p2p.Install(meshNodes.Get(1), busNodes.Get(0));
    // Connect bus node 3 <-> line node 0
    NetDeviceContainer br3 = p2p.Install(busNodes.Get(3), lineNodes.Get(0));
    // Connect line node 3 <-> starHub
    NetDeviceContainer br4 = p2p.Install(lineNodes.Get(3), starHub.Get(0));
    // Connect star spoke 3 <-> tree root
    NetDeviceContainer br5 = p2p.Install(starSpokes.Get(3), treeLevel0.Get(0));

    // Install Internet stack & OSPF
    Ipv4OspfRoutingHelper ospfRouting;
    InternetStackHelper stack;
    stack.SetRoutingHelper(ospfRouting);

    stack.Install(allNodes);

    // Assign IP addresses
    uint32_t netCount = 0;
    char netbuf[100];

    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> allIfaces;

    // Ring
    for (uint32_t i = 0; i < 4; ++i)
    {
        snprintf(netbuf, sizeof(netbuf), "10.%u.1.0", ++netCount);
        address.SetBase(netbuf, "255.255.255.0");
        allIfaces.push_back(address.Assign(ringDevices[i]));
    }

    // Mesh
    for (uint32_t i = 0; i < 4; ++i)
    {
        for (uint32_t j = i + 1; j < 4; ++j)
        {
            snprintf(netbuf, sizeof(netbuf), "10.%u.1.0", ++netCount);
            address.SetBase(netbuf, "255.255.255.0");
            allIfaces.push_back(address.Assign(meshDevices[i][j]));
        }
    }

    // Bus
    for (uint32_t i = 0; i < 3; ++i)
    {
        snprintf(netbuf, sizeof(netbuf), "10.%u.1.0", ++netCount);
        address.SetBase(netbuf, "255.255.255.0");
        allIfaces.push_back(address.Assign(busDevices[i]));
    }

    // Line
    for (uint32_t i = 0; i < 3; ++i)
    {
        snprintf(netbuf, sizeof(netbuf), "10.%u.1.0", ++netCount);
        address.SetBase(netbuf, "255.255.255.0");
        allIfaces.push_back(address.Assign(lineDevices[i]));
    }

    // Star
    for (uint32_t i = 0; i < 4; ++i)
    {
        snprintf(netbuf, sizeof(netbuf), "10.%u.1.0", ++netCount);
        address.SetBase(netbuf, "255.255.255.0");
        allIfaces.push_back(address.Assign(starDevices[i]));
    }

    // Tree
    for (uint32_t i = 0; i < 2; ++i)
    {
        snprintf(netbuf, sizeof(netbuf), "10.%u.1.0", ++netCount);
        address.SetBase(netbuf, "255.255.255.0");
        allIfaces.push_back(address.Assign(treeDevicesL0L1[i]));
    }
    for (uint32_t i = 0; i < 4; ++i)
    {
        snprintf(netbuf, sizeof(netbuf), "10.%u.1.0", ++netCount);
        address.SetBase(netbuf, "255.255.255.0");
        allIfaces.push_back(address.Assign(treeDevicesL1L2[i]));
    }

    // Inter-topology bridges
    snprintf(netbuf, sizeof(netbuf), "10.%u.1.0", ++netCount);
    address.SetBase(netbuf, "255.255.255.0");
    allIfaces.push_back(address.Assign(br1));
    snprintf(netbuf, sizeof(netbuf), "10.%u.1.0", ++netCount);
    address.SetBase(netbuf, "255.255.255.0");
    allIfaces.push_back(address.Assign(br2));
    snprintf(netbuf, sizeof(netbuf), "10.%u.1.0", ++netCount);
    address.SetBase(netbuf, "255.255.255.0");
    allIfaces.push_back(address.Assign(br3));
    snprintf(netbuf, sizeof(netbuf), "10.%u.1.0", ++netCount);
    address.SetBase(netbuf, "255.255.255.0");
    allIfaces.push_back(address.Assign(br4));
    snprintf(netbuf, sizeof(netbuf), "10.%u.1.0", ++netCount);
    address.SetBase(netbuf, "255.255.255.0");
    allIfaces.push_back(address.Assign(br5));

    // UDP Echo Server on tree leaf
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApp = echoServer.Install(treeLevel2.Get(3));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    // UDP Echo Client on ring node
    UdpEchoClientHelper echoClient(Ipv4Address("10.24.1.2"), echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(3));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(64));
    ApplicationContainer clientApp = echoClient.Install(ringNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(20.0));

    // Enable tracing
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("hybrid-topology.tr"));
    p2p.EnablePcapAll("hybrid-topology");

    // NetAnim: Assign positions
    AnimationInterface anim("hybrid-topology.xml");
    // Ring nodes (circle)
    for (uint32_t i = 0; i < 4; ++i)
    {
        double angle = 2 * M_PI * i / 4;
        double x = 60 + 20 * std::cos(angle);
        double y = 60 + 20 * std::sin(angle);
        anim.SetConstantPosition(ringNodes.Get(i), x, y);
    }
    // Mesh nodes (square)
    anim.SetConstantPosition(meshNodes.Get(0), 20, 20);
    anim.SetConstantPosition(meshNodes.Get(1), 40, 20);
    anim.SetConstantPosition(meshNodes.Get(2), 20, 40);
    anim.SetConstantPosition(meshNodes.Get(3), 40, 40);
    // Bus nodes (horizontal)
    for (uint32_t i = 0; i < 4; ++i)
    {
        anim.SetConstantPosition(busNodes.Get(i), 40 + i * 15, 60);
    }
    // Line nodes (vertical)
    for (uint32_t i = 0; i < 4; ++i)
    {
        anim.SetConstantPosition(lineNodes.Get(i), 100, 40 + i * 10);
    }
    // Star hub & spokes
    anim.SetConstantPosition(starHub.Get(0), 130, 60);
    for (uint32_t i = 0; i < 4; ++i)
    {
        double angle = 2 * M_PI * i / 4;
        double x = 130 + 20 * std::cos(angle);
        double y = 60 + 20 * std::sin(angle);
        anim.SetConstantPosition(starSpokes.Get(i), x, y);
    }
    // Tree (rooted at bottom right)
    anim.SetConstantPosition(treeLevel0.Get(0), 160, 90);
    anim.SetConstantPosition(treeLevel1.Get(0), 150, 110);
    anim.SetConstantPosition(treeLevel1.Get(1), 170, 110);
    anim.SetConstantPosition(treeLevel2.Get(0), 145, 130);
    anim.SetConstantPosition(treeLevel2.Get(1), 155, 130);
    anim.SetConstantPosition(treeLevel2.Get(2), 165, 130);
    anim.SetConstantPosition(treeLevel2.Get(3), 175, 130);

    Simulator::Stop(Seconds(22.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}