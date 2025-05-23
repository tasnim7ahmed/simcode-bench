#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"

int main (int argc, char *argv[])
{
    // Create nodes
    NodeContainer enbNodes;
    enbNodes.Create (1);
    NodeContainer ueNodes;
    ueNodes.Create (1);

    // Create LTE and EPC helpers
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
    lteHelper->SetEpcHelper (epcHelper);

    // Get PGW node (part of EPC setup)
    Ptr<Node> pgw = epcHelper->GetPgwNode ();

    // Install mobility model (static position for simplicity)
    MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (enbNodes);
    mobility.Install (ueNodes);

    // Install LTE devices on nodes
    NetDeviceContainer enbDevs;
    enbDevs = lteHelper->InstallEnbDevice (enbNodes);
    NetDeviceContainer ueDevs;
    ueDevs = lteHelper->InstallUeDevice (ueNodes);

    // Install the IP stack on UE and eNB (for application communication)
    // UE needs an IP stack to run the UDP server
    InternetStackHelper internet;
    internet.Install (ueNodes);
    // eNB needs an IP stack to run the UDP client
    internet.Install (enbNodes); 

    // Assign IP addresses to UEs. The EPC helper handles this.
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueDevs));
    Ipv4Address ueAddress = ueIpIface.GetAddress (0); // Get the UE's assigned IP address

    // Attach UEs to eNBs
    lteHelper->Attach (ueDevs, enbDevs.Get (0));

    // Activate default EPS bearer for data communication
    enum EpsBearer::Qci qci = EpsBearer::NGBR_VIDEO_CALL_INTERACTIVE;
    EpsBearer bearer (qci);
    lteHelper->ActivateDataRadioBearer (ueDevs, bearer);

    // Application parameters
    uint16_t port = 12345;
    uint32_t packetSize = 1024; // Bytes
    uint32_t numPackets = 1000;
    Time interPacketInterval = MilliSeconds (20);

    // UDP Server application (on UE)
    UdpServerHelper server (port);
    ApplicationContainer serverApps = server.Install (ueNodes.Get (0));
    serverApps.Start (Seconds (1.0)); // Start server after simulation initialization
    serverApps.Stop (Seconds (25.0)); // Ensure server runs long enough for client to finish

    // UDP Client application (on eNB)
    // The client sends to the UE's IP address on the specified port
    UdpClientHelper client (ueAddress, port);
    client.SetAttribute ("MaxPackets", UintegerValue (numPackets));
    client.SetAttribute ("Interval", TimeValue (interPacketInterval));
    client.SetAttribute ("PacketSize", UintegerValue (packetSize));
    ApplicationContainer clientApps = client.Install (enbNodes.Get (0));
    clientApps.Start (Seconds (2.0)); // Start client after server is ready
    clientApps.Stop (Seconds (25.0)); // Client will stop once MaxPackets are sent

    // Set overall simulation stop time
    // Client sends 1000 packets at 20ms interval = 20 seconds of transmission.
    // If client starts at 2.0s, it will finish sending by 22.0s.
    // Setting stop time to 25.0s provides a safe buffer.
    Simulator::Stop (Seconds (25.0));

    // Run the simulation
    Simulator::Run ();

    // Clean up simulation resources
    Simulator::Destroy ();

    return 0;
}