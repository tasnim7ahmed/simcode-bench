#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ospf-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridTopologySimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes for each topology segment
    NodeContainer ringNodes;
    ringNodes.Create(5);

    NodeContainer meshNodes;
    meshNodes.Create(4);

    NodeContainer busNodes;
    busNodes.Create(5);

    NodeContainer lineNodes;
    lineNodes.Create(5);

    NodeContainer starHub;
    starHub.Create(1);

    NodeContainer starSpokes;
    starSpokes.Create(5);

    NodeContainer treeRoot;
    treeRoot.Create(1);

    NodeContainer treeLevel1;
    treeLevel1.Create(2);

    NodeContainer treeLevel2;
    treeLevel2.Create(4);

    // Combine all nodes into a global container
    NodeContainer allNodes;
    allNodes.Add(ringNodes);
    allNodes.Add(meshNodes);
    allNodes.Add(busNodes);
    allNodes.Add(lineNodes);
    allNodes.Add(starHub);
    allNodes.Add(starSpokes);
    allNodes.Add(treeRoot);
    allNodes.Add(treeLevel1);
    allNodes.Add(treeLevel2);

    // Setup PointToPoint links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer ringDevices;
    for (uint32_t i = 0; i < ringNodes.GetN(); ++i) {
        NetDeviceContainer dev = p2p.Install(ringNodes.Get(i), ringNodes.Get((i + 1) % ringNodes.GetN()));
        ringDevices.Add(dev);
    }

    NetDeviceContainer meshDevices;
    for (uint32_t i = 0; i < meshNodes.GetN(); ++i) {
        for (uint32_t j = i + 1; j < meshNodes.GetN(); ++j) {
            NetDeviceContainer dev = p2p.Install(meshNodes.Get(i), meshNodes.Get(j));
            meshDevices.Add(dev);
        }
    }

    NetDeviceContainer busDevices;
    for (uint32_t i = 0; i < busNodes.GetN() - 1; ++i) {
        NetDeviceContainer dev = p2p.Install(busNodes.Get(i), busNodes.Get(i + 1));
        busDevices.Add(dev);
    }

    NetDeviceContainer lineDevices;
    for (uint32_t i = 0; i < lineNodes.GetN() - 1; ++i) {
        NetDeviceContainer dev = p2p.Install(lineNodes.Get(i), lineNodes.Get(i + 1));
        lineDevices.Add(dev);
    }

    NetDeviceContainer starDevices;
    for (uint32_t i = 0; i < starSpokes.GetN(); ++i) {
        NetDeviceContainer dev = p2p.Install(starHub.Get(0), starSpokes.Get(i));
        starDevices.Add(dev);
    }

    NetDeviceContainer treeDevices;
    treeDevices.Add(p2p.Install(treeRoot.Get(0), treeLevel1.Get(0)));
    treeDevices.Add(p2p.Install(treeRoot.Get(0), treeLevel1.Get(1)));

    treeDevices.Add(p2p.Install(treeLevel1.Get(0), treeLevel2.Get(0)));
    treeDevices.Add(p2p.Install(treeLevel1.Get(0), treeLevel2.Get(1)));
    treeDevices.Add(p2p.Install(treeLevel1.Get(1), treeLevel2.Get(2)));
    treeDevices.Add(p2p.Install(treeLevel1.Get(1), treeLevel2.Get(3)));

    // Install Internet Stack with OSPF
    InternetStackHelper internet;
    OspfHelper ospfRoutingHelper;
    internet.SetRoutingHelper(ospfRoutingHelper);
    internet.Install(allNodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.0.0", "255.255.255.0");
    ipv4.Assign(ringDevices);

    ipv4.NewNetwork();
    ipv4.Assign(meshDevices);

    ipv4.NewNetwork();
    ipv4.Assign(busDevices);

    ipv4.NewNetwork();
    ipv4.Assign(lineDevices);

    ipv4.NewNetwork();
    ipv4.Assign(starDevices);

    ipv4.NewNetwork();
    ipv4.Assign(treeDevices);

    // Enable NetAnim visualization
    AnimationInterface anim("hybrid-topology.xml");
    anim.EnablePacketMetadata(true);
    anim.EnableIpv4RouteTracking("routing-table-writer.xml", Seconds(0), Seconds(20), Seconds(0.25));

    // Set node positions for animation
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(20),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(allNodes);

    // Echo server setup on the first node of each topology
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps;

    serverApps.Add(echoServer.Install(ringNodes.Get(0)));
    serverApps.Add(echoServer.Install(meshNodes.Get(0)));
    serverApps.Add(echoServer.Install(busNodes.Get(0)));
    serverApps.Add(echoServer.Install(lineNodes.Get(0)));
    serverApps.Add(echoServer.Install(starHub.Get(0)));
    serverApps.Add(echoServer.Install(treeRoot.Get(0)));

    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(20.0));

    // Echo client setup sending to servers
    UdpEchoClientHelper echoClient(Ipv4Address(), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;

    echoClient.SetRemote(ringNodes.Get(0)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), 9);
    clientApps.Add(echoClient.Install(ringNodes.Get(1)));

    echoClient.SetRemote(meshNodes.Get(0)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), 9);
    clientApps.Add(echoClient.Install(meshNodes.Get(1)));

    echoClient.SetRemote(busNodes.Get(0)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), 9);
    clientApps.Add(echoClient.Install(busNodes.Get(1)));

    echoClient.SetRemote(lineNodes.Get(0)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), 9);
    clientApps.Add(echoClient.Install(lineNodes.Get(1)));

    echoClient.SetRemote(starHub.Get(0)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), 9);
    clientApps.Add(echoClient.Install(starSpokes.Get(1)));

    echoClient.SetRemote(treeRoot.Get(0)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), 9);
    clientApps.Add(echoClient.Install(treeLevel1.Get(1)));

    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(20.0));

    Simulator::Stop(Seconds(25.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}