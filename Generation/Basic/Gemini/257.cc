#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

// NS_LOG_COMPONENT_DEFINE("LteUdpGridSimulation"); // Don't define if not using in this file

int main(int argc, char *argv[])
{
    // Enable logging (optional, but good for debugging)
    // LogComponentEnable("LteUdpGridSimulation", LOG_LEVEL_INFO);
    // LogComponentEnable("LteHelper", LOG_LEVEL_INFO);
    // LogComponentEnable("EpcHelper", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpServerApplication", LOG_LEVEL_INFO);

    // Command line arguments (optional, useful for customization)
    // CommandLine cmd(__FILE__);
    // cmd.AddValue("simTime", "Total duration of the simulation in seconds", 10.0);
    // cmd.Parse(argc, argv);

    double simTime = 10.0; // Simulation time in seconds

    // Create LTE and EPC helpers
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Configure the EPC MME/SGW mobility model type
    lteHelper->SetEnbDeviceAttribute("MmeSgwMobilityModelType", StringValue("ns3::ConstantPositionMobilityModel"));

    // Create nodes for eNodeB and UEs
    NodeContainer enbNodes;
    enbNodes.Create(1); // One eNodeB
    NodeContainer ueNodes;
    ueNodes.Create(2); // Two UEs (UE1 and UE2)

    // Install Mobility model for nodes in a grid layout
    // The nodes will be placed in the order they are added to the container passed to Install().
    // We will place eNB, then UE1, then UE2.
    NodeContainer allNodes;
    allNodes.Add(enbNodes.Get(0)); // First node in the grid will be the eNB
    allNodes.Add(ueNodes.Get(0));  // Second node will be UE1 (client)
    allNodes.Add(ueNodes.Get(1));  // Third node will be UE2 (server)

    GridMobilityHelper gridMobility;
    gridMobility.SetResolution(50.0); // Distance between nodes in meters
    gridMobility.SetDimensions(1, 3); // 1 row, 3 columns
    gridMobility.SetPosition(0, 0, 0); // Starting point of the grid (bottom-left corner)
    gridMobility.Install(allNodes);

    // Install LTE devices on eNodeB and UEs
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack on the UEs and eNodeBs
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    // Assign IP addresses to UEs
    Ipv4InterfaceContainer ueIpIface = epcHelper->AssignUeIpv4Address(ueLteDevs);

    // Attach UEs to the eNodeB
    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0)); // Attach UE1
    lteHelper->Attach(ueLteDevs.Get(1), enbLteDevs.Get(0)); // Attach UE2

    // Activate a data radio bearer (DRB)
    enum EpsBearer::Qci qci = EpsBearer::NGBR_VIDEO_CALL_1;
    EpsBearer bearer(qci);
    lteHelper->ActivateDataRadioBearer(ueLteDevs, bearer);

    // Setup UDP Applications
    // UE2 acts as UDP server
    uint16_t port = 9; // Discard port
    UdpServerHelper serverHelper(port);
    ApplicationContainer serverApps = serverHelper.Install(ueNodes.Get(1)); // Install on UE2
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime + 1.0)); // Stop after simulation ends to allow final processing

    // UE1 acts as UDP client, sending to UE2
    Ipv4Address ue2Ip = ueIpIface.GetAddress(1); // IP address of UE2 (server)
    UdpClientHelper clientHelper(ue2Ip, port);
    clientHelper.SetAttribute("MaxPackets", UintegerValue(2));      // Send 2 packets
    clientHelper.SetAttribute("Interval", TimeValue(Seconds(1.0))); // 1-second interval between packets
    clientHelper.SetAttribute("PacketSize", UintegerValue(512));    // 512 bytes per packet

    ApplicationContainer clientApps = clientHelper.Install(ueNodes.Get(0)); // Install on UE1
    clientApps.Start(Seconds(1.0)); // Start client after 1 second
    clientApps.Stop(Seconds(simTime));

    // Enable LTE traces
    lteHelper->EnableTraces();

    // Set simulation stop time
    Simulator::Stop(Seconds(simTime));

    // Run simulation
    Simulator::Run();

    // Cleanup
    Simulator::Destroy();

    return 0;
}