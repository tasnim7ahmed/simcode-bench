#include "ns3/lte-helper.h"
#include "ns3/epc-helper.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging for relevant components to observe simulation events
    LogComponentEnable("LteHandoverSap", LOG_LEVEL_INFO); // Handover service access point events
    LogComponentEnable("LteEnbRrc", LOG_LEVEL_INFO);      // eNodeB RRC layer for handover decisions
    LogComponentEnable("LteUeRrc", LOG_LEVEL_INFO);       // UE RRC layer for connection management
    LogComponentEnable("LteUePhy", LOG_LEVEL_INFO);       // UE physical layer events (e.g., measurements)
    LogComponentEnable("LteEnbPhy", LOG_LEVEL_INFO);      // eNodeB physical layer events
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);      // UDP client application logs
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);      // UDP server application logs

    // Simulation parameters
    double simTime = 10.0; // Total duration of the simulation in seconds
    uint16_t dlPort = 3000; // Port for UDP downlink data transfer (from remote host to UE)

    // Command line argument parsing
    CommandLine cmd(__FILE__);
    cmd.AddValue("simTime", "Total duration of the simulation in seconds", simTime);
    cmd.Parse(argc, argv);

    // 1. Create LTE and EPC helpers
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Enable X2-based handover (for intra-MME/SGW handover, provides faster handover)
    lteHelper->EnableX2Handover();
    // Enable data forwarding during handover (critical for seamless data transfer during HO)
    lteHelper->EnableDatahandover();

    // Configure Handover Algorithm: A3RsrpHandoverAlgorithm
    // Handover is triggered when a neighboring cell's RSRP (Reference Signal Received Power)
    // becomes a configured offset better than the serving cell's RSRP.
    lteHelper->SetHandoverAlgorithmType("ns3::A3RsrpHandoverAlgorithm");
    // Neighbor must be 3dB (or 3 units in RSRP) better than serving cell to trigger A3 event
    Config::SetDefault("ns3::LteEnbRrc::HandoverConfigA3Offset", DoubleValue(3.0));
    // Periodicity of UE measurements reported to eNodeB
    Config::SetDefault("ns3::LteEnbRrc::EutranRrc::HandoverMeasurementPeriod", TimeValue(MilliSeconds(100)));
    // Time for which handover conditions must be met before triggering HO procedure
    Config::SetDefault("ns3::LteEnbRrc::EutranRrc::HandoverTimeToTrigger", TimeValue(MilliSeconds(10)));

    // 2. Create eNodeB and UE nodes
    NodeContainer enbNodes;
    enbNodes.Create(2); // Two eNodeBs
    NodeContainer ueNodes;
    ueNodes.Create(1);  // One UE

    // 3. Create a remote host (acting as a server) and connect it to the PGW (Packet Gateway)
    // This host will be external to the LTE access network, simulating a server on the internet
    // or an external data network, accessed by UEs via the EPC.
    Ptr<Node> remoteHost = CreateObject<Node>();
    InternetStackHelper internet;
    internet.Install(remoteHost); // Install Internet stack on the remote host

    // Connect remote host to PGW using a Point-to-Point link
    Ptr<PointToPointHelper> p2pHelper = CreateObject<PointToPointHelper>();
    p2pHelper->SetDeviceAttribute("DataRate", StringValue("100Gbps")); // High bandwidth link
    p2pHelper->SetChannelAttribute("Delay", StringValue("1ms"));      // Small delay
    NetDeviceContainer internetDevices = p2pHelper->Install(epcHelper->GetPgwNode(), remoteHost);

    // Assign IP addresses for the PGW-remoteHost link
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("10.0.0.0", "255.255.255.252"); // A small dedicated subnet
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1); // IP address of the remoteHost

    // Add route to the UE IP address pool for the remote host.
    // This allows the remote host to send traffic towards the UEs connected to the APN (Access Point Name)
    // via the PGW.
    epcHelper->AddUeApnRoute(remoteHost, "ns3::Apn::DEFAULT_APN");

    // 4. Install LTE devices on eNodeBs and UEs
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // 5. Install Internet stack on eNodeBs and UEs
    internet.Install(enbNodes);
    internet.Install(ueNodes);

    // Assign IP address to UE (managed by the EPC)
    Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address(ueLteDevs);

    // 6. Set up mobility for eNodeBs and UE
    MobilityHelper mobility;

    // eNodeB positions (fixed positions)
    Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator>();
    enbPositionAlloc->Add(Vector(0.0, 0.0, 0.0));   // eNodeB 0 at origin
    enbPositionAlloc->Add(Vector(500.0, 0.0, 0.0)); // eNodeB 1 at 500m along X-axis
    mobility.SetPositionAllocator(enbPositionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);

    // UE mobility (RandomWalk2dMobilityModel)
    // The UE starts near eNodeB 0 and performs a random walk within bounds that cover both eNodeBs,
    // facilitating handover.
    Ptr<RandomRectanglePositionAllocator> uePositionAlloc = CreateObject<RandomRectanglePositionAllocator>();
    // Initial random position for UE within a box near eNodeB 0 (0 to 50m X, -25 to 25m Y)
    uePositionAlloc->SetAttribute("X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
    uePositionAlloc->SetAttribute("Y", StringValue("ns3::UniformRandomVariable[Min=-25.0|Max=25.0]"));
    uePositionAlloc->SetAttribute("Z", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    mobility.SetPositionAllocator(uePositionAlloc);

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-100.0, 600.0, -200.0, 200.0)), // Larger bounds for the random walk
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=10.0]"), // Speed of 10 m/s (36 km/h)
                              "Distance", DoubleValue(50.0)); // Change direction every 50 meters
    mobility.Install(ueNodes);

    // 7. Attach UE to an eNodeB initially
    // UE is initially attached to eNodeB 0
    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

    // 8. Setup UDP Applications for data transfer
    // UDP Server application on the remoteHost
    UdpServerHelper server(dlPort);
    ApplicationContainer serverApps = server.Install(remoteHost);
    serverApps.Start(Seconds(0.0)); // Start server at simulation start
    serverApps.Stop(Seconds(simTime)); // Stop server at simulation end

    // UDP Client application on the UE, sending data to the remoteHost
    UdpClientHelper client(remoteHostAddr, dlPort);
    client.SetAttribute("MaxPackets", UintegerValue(100000)); // Max packets to send (sufficient for 10s)
    client.SetAttribute("Interval", TimeValue(MilliSeconds(10))); // Send a packet every 10ms (100 packets/sec)
    client.SetAttribute("PacketSize", UintegerValue(1024)); // 1KB packet size
    ApplicationContainer clientApps = client.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(0.1)); // Start client slightly after server to ensure server is ready
    clientApps.Stop(Seconds(simTime)); // Stop client at simulation end

    // 9. Enable LTE traces for Handover events and other statistics
    lteHelper->EnableTraces(); // Enable general LTE traces (e.g., RLC, MAC, Phy traces)
    // Handover-specific traces are enabled via LogComponentEnable at the top

    // 10. NetAnim Visualization
    AnimationInterface anim("lte_handover.xml");
    // Set static positions for eNodeBs, PGW, and Server for visualization in NetAnim
    anim.SetConstantPosition(enbNodes.Get(0), 0, 0);
    anim.SetConstantPosition(enbNodes.Get(1), 500, 0);
    anim.SetConstantPosition(epcHelper->GetPgwNode(), -100, 100); // Place PGW off to the side
    anim.SetConstantPosition(remoteHost, -200, 100);             // Place Server next to PGW

    // Optional: Set descriptive names and colors for nodes in NetAnim for better readability
    anim.UpdateNodeDescription(enbNodes.Get(0), "eNB-0");
    anim.UpdateNodeColor(enbNodes.Get(0), 0, 0, 255); // Blue for eNodeBs
    anim.UpdateNodeDescription(enbNodes.Get(1), "eNB-1");
    anim.UpdateNodeColor(enbNodes.Get(1), 0, 0, 255);
    anim.UpdateNodeDescription(ueNodes.Get(0), "UE");
    anim.UpdateNodeColor(ueNodes.Get(0), 255, 0, 0); // Red for UE
    anim.UpdateNodeDescription(epcHelper->GetPgwNode(), "PGW");
    anim.UpdateNodeColor(epcHelper->GetPgwNode(), 0, 255, 0); // Green for PGW
    anim.UpdateNodeDescription(remoteHost, "Server");
    anim.UpdateNodeColor(remoteHost, 255, 255, 0); // Yellow for Server

    // Enable packet metadata and IPv4 route tracking for detailed NetAnim visualization
    anim.EnablePacketMetadata();
    anim.EnableIpv4RouteTracking();

    // 11. Run simulation
    Simulator::Stop(Seconds(simTime)); // Stop simulation after the specified time
    Simulator::Run();                  // Execute the simulation
    Simulator::Destroy();              // Clean up allocated memory

    return 0;
}