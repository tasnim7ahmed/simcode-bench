#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h" // For PointToPointEpcHelper

int main (int argc, char *argv[])
{
    // Create LTE and EPC helpers
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
    lteHelper->SetEpcHelper (epcHelper);

    // Set up a mobility model for eNodeB and UEs
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    positionAlloc->Add (ns3::Vector (0.0, 0.0, 0.0)); // eNodeB at (0,0,0)
    positionAlloc->Add (ns3::Vector (5.0, 0.0, 0.0)); // UE 0 at (5,0,0)
    positionAlloc->Add (ns3::Vector (10.0, 0.0, 0.0)); // UE 1 at (10,0,0)

    MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.SetPositionAllocator (positionAlloc);

    // Create nodes
    NodeContainer enbNodes;
    enbNodes.Create (1);
    mobility.Install (enbNodes); // Install mobility for eNodeB

    NodeContainer ueNodes;
    ueNodes.Create (2);
    mobility.Install (ueNodes); // Install mobility for UEs

    // Install Internet stacks on UEs
    InternetStackHelper internet;
    internet.Install (ueNodes);

    // Install LTE devices
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

    // Attach UEs to eNodeB
    lteHelper->Attach (ueLteDevs.Get (0), enbLteDevs.Get (0)); // Attach UE 0
    lteHelper->Attach (ueLteDevs.Get (1), enbLteDevs.Get (0)); // Attach UE 1

    // Assign IP addresses to UEs
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));

    // Get UE 0's IP address (for server)
    ns3::Ipv4Address ue0Ipv4Address = ueIpIface.GetAddress (0);

    // Setup Applications
    uint16_t port = 8080;
    ns3::Time serverStartTime = ns3::Seconds (1.0);
    ns3::Time clientStartTime = ns3::Seconds (2.0);
    ns3::Time stopTime = ns3::Seconds (10.0);

    // Server (UE 0)
    // PacketSinkHelper listens on the specified port
    ns3::PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory",
                                      ns3::InetSocketAddress (ns3::Ipv4Address::Any, port));
    ns3::ApplicationContainer serverApps = sinkHelper.Install (ueNodes.Get (0));
    serverApps.Start (serverStartTime);
    serverApps.Stop (stopTime);

    // Client (UE 1)
    // BulkSendHelper sends a fixed amount of data over TCP
    ns3::BulkSendHelper clientHelper ("ns3::TcpSocketFactory",
                                      ns3::InetSocketAddress (ue0Ipv4Address, port));
    clientHelper.SetAttribute ("MaxBytes", ns3::UintegerValue (1000000)); // Send 1,000,000 bytes
    ns3::ApplicationContainer clientApps = clientHelper.Install (ueNodes.Get (1));
    clientApps.Start (clientStartTime);
    clientApps.Stop (stopTime);

    // Run simulation
    ns3::Simulator::Stop (stopTime);
    ns3::Simulator::Run ();
    ns3::Simulator::Destroy ();

    return 0;
}