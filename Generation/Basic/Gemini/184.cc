#include "ns3/lte-helper.h"
#include "ns3/epc-helper.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

int main(int argc, char *argv[])
{
    // Set default attributes for LTE and EPC
    Config::SetDefault("ns3::LteUeNetDevice::DlBandwidth", UintegerValue(50)); // Example: 50 RBs
    Config::SetDefault("ns3::LteUeNetDevice::UlBandwidth", UintegerValue(50));
    Config::SetDefault("ns3::LteEnbNetDevice::DlBandwidth", UintegerValue(50));
    Config::SetDefault("ns3::LteEnbNetDevice::UlBandwidth", UintegerValue(50));

    // Command line arguments (optional, but good practice for simulation time)
    double simTime = 10.0; // seconds
    CommandLine cmd(__FILE__);
    cmd.AddValue("simTime", "Total duration of the simulation in seconds", simTime);
    cmd.Parse(argc, argv);

    // Create LTE and EPC helpers
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<EpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>(); // Use PointToPointEpcHelper for simplicity

    lteHelper->SetEpcHelper(epcHelper);

    // Get the PGW node from the EPC helper
    Ptr<Node> pgw = epcHelper->GetPgwNode();

    // Create a remote host and connect it to the PGW via a P2P link
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internet;
    internet.Install(remoteHost);

    // Create the P2P link between PGW and remote host
    PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2pHelper.SetChannelAttribute("Delay", StringValue("10ms"));
    NetDeviceContainer internetDevices = p2pHelper.Install(pgw, remoteHost);

    // Assign IP addresses to the P2P link
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1); // IP address of the remote host

    // Add a static route to the remote host to reach the UE's network (7.0.0.0/8 default for PointToPointEpcHelper)
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.Get(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddHostRouteTo(Ipv4Address("7.0.0.0"), Ipv4Address("10.1.2.1"), 1); // Destination, Gateway, Interface Index

    // Create eNodeB and UE nodes
    NodeContainer enbNodes;
    enbNodes.Create(1); // One eNodeB
    NodeContainer ueNodes;
    ueNodes.Create(2); // Two UEs

    // Install the Internet stack on eNodeBs and UEs
    internet.Install(enbNodes);
    internet.Install(ueNodes);

    // Install LTE devices on eNodeBs and UEs
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Attach UEs to the eNodeB
    lteHelper->AttachUe(ueLteDevs.Get(0), enbLteDevs.Get(0));
    lteHelper->AttachUe(ueLteDevs.Get(1), enbLteDevs.Get(0));

    // Assign IP addresses to UEs (handled by EPC, but needed to query later if desired)
    // The UEs will automatically get IPs from the EPC (e.g., 7.0.0.x range) when attached.
    // Ipv4InterfaceContainer ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));


    // Configure UDP applications
    uint16_t remotePort = 9; // Discard port for server
    uint32_t packetSize = 1024; // Bytes
    DataRate dataRate("1Mbps");
    uint32_t numPackets = 100000; // A large number to ensure continuous traffic

    // Install UDP Server on the remote host
    UdpServerHelper udpServer(remotePort);
    ApplicationContainer serverApps = udpServer.Install(remoteHost);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime));

    // Install UDP Client on UE 0
    UdpClientHelper udpClient0(remoteHostAddr, remotePort);
    udpClient0.SetAttribute("MaxPackets", UintegerValue(numPackets));
    udpClient0.SetAttribute("Interval", TimeValue(packetSize * 8 / dataRate));
    udpClient0.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApps0 = udpClient0.Install(ueNodes.Get(0));
    clientApps0.Start(Seconds(1.0)); // Start at 1 sec
    clientApps0.Stop(Seconds(simTime));

    // Install UDP Client on UE 1
    UdpClientHelper udpClient1(remoteHostAddr, remotePort);
    udpClient1.SetAttribute("MaxPackets", UintegerValue(numPackets));
    udpClient1.SetAttribute("Interval", TimeValue(packetSize * 8 / dataRate));
    udpClient1.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApps1 = udpClient1.Install(ueNodes.Get(1));
    clientApps1.Start(Seconds(1.5)); // Start at 1.5 sec (offset for visualization)
    clientApps1.Stop(Seconds(simTime));

    // Enable NetAnim tracing
    std::string animFile = "lte_simple_netanim.xml";
    NetAnimHelper netanim;
    netanim.Set
    netanim.SetTraceFile(animFile);
    netanim.EnablePacketMetadata(true);
    netanim.EnableIpv4RouteTracking(true);
    netanim.TraceFx();

    // Set simulation stop time
    Simulator::Stop(Seconds(simTime));

    // Run simulation
    Simulator::Run();

    // Clean up
    Simulator::Destroy();

    return 0;
}