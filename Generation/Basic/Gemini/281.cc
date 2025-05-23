#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Configure command line arguments (optional, but good practice)
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Set up logging for relevant modules
    LogComponentEnable("LteHelper", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("MobilityHelper", LOG_LEVEL_INFO);
    LogComponentEnable("RandomWalk2dMobilityModel", LOG_LEVEL_INFO);

    // Create LTE helper and EPC helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    // Use PointToPointEpcHelper for simplicity, connecting eNodeBs to a single PGW
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Create eNodeB and UE nodes
    NodeContainer enbNodes;
    enbNodes.Create(1); // One eNodeB
    NodeContainer ueNodes;
    ueNodes.Create(3);  // Three UEs

    // Install mobility for eNodeB: fixed at the center of the 50x50 area
    Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator>();
    enbPositionAlloc->Add(Vector(25.0, 25.0, 0.0)); // eNodeB at (25, 25, 0)
    MobilityHelper enbMobility;
    enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbMobility.SetPositionAllocator(enbPositionAlloc);
    enbMobility.Install(enbNodes);

    // Install mobility for UEs: Random Walk in a 50x50 area
    MobilityHelper ueMobility;
    ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                 "Bounds", RectangleValue(0.0, 50.0, 0.0, 50.0),
                                 "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"), // 1 m/s constant speed
                                 "Time", StringValue("ns3::ConstantRandomVariable[Constant=1.0]")); // Change direction every 1 second
    ueMobility.Install(ueNodes);

    // Install Internet Stack on all nodes
    // eNodeB will have an interface connecting to the EPC
    // UEs will receive IP addresses from the PGW through the LTE network
    InternetStackHelper internet;
    internet.Install(enbNodes);
    internet.Install(ueNodes);

    // Install LTE devices on nodes
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Attach UEs to the eNodeB
    // All UEs are attached to the single eNodeB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        lteHelper->Attach(ueLteDevs.Get(i), enbLteDevs.Get(0));
    }

    // Create a remote host (server) connected to the PGW
    // This is the standard way to deploy a server for LTE UEs in ns-3
    Ptr<Node> pgw = epcHelper->GetPgwNode();

    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0); // The server node
    internet.Install(remoteHost); // Install Internet Stack on the server node

    // Create a point-to-point link between the PGW and the remote host
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));
    NetDeviceContainer internetDevices = p2p.Install(pgw, remoteHost);

    // Assign IP addresses to the point-to-point link
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1); // Get IP address of the remote host

    // Populate routing tables for the server node to reach UEs
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Applications: UDP Server on remoteHost, UDP Clients on UEs
    uint16_t ulPort = 9; // UDP server port as specified in the problem description

    // Install UDP server application on the remote host
    UdpServerHelper server(ulPort);
    ApplicationContainer serverApps = server.Install(remoteHost);
    serverApps.Start(Seconds(0.0));  // Server starts at simulation beginning
    serverApps.Stop(Seconds(10.0));  // Server stops at simulation end (10s)

    // Install UDP client applications on UEs
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ptr<Node> ueNode = ueNodes.Get(i);
        UdpClientHelper client(remoteHostAddr, ulPort); // Target the server's IP and port
        client.SetAttribute("MaxPackets", UintegerValue(10)); // Send 10 packets
        client.SetAttribute("Interval", TimeValue(Seconds(1.0))); // 1-second intervals
        client.SetAttribute("PacketSize", UintegerValue(1024)); // Example packet size

        ApplicationContainer clientApps = client.Install(ueNode);
        // Clients start at 1.0s to send packets at 1.0s, 2.0s, ..., 10.0s (total 10 packets)
        clientApps.Start(Seconds(1.0));
        clientApps.Stop(Seconds(10.0)); // Last packet scheduled to be sent at 10.0s
    }

    // Set simulation stop time
    Simulator::Stop(Seconds(10.0)); // The simulation runs for 10 seconds

    // Run the simulation
    Simulator::Run();

    // Clean up simulation resources
    Simulator::Destroy();

    return 0;
}