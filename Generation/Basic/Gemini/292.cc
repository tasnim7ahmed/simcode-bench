#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("NrOneGnbOneUeMobility");

int main (int argc, char *argv[])
{
    // Enable logging for relevant components
    LogComponentEnable ("NrOneGnbOneUeMobility", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable ("NrGnbNetDevice", LOG_LEVEL_INFO);
    LogComponentEnable ("NrUeNetDevice", LOG_LEVEL_INFO);
    LogComponentEnable ("NrHelper", LOG_LEVEL_INFO);

    // Simulation parameters
    double simTime = 10.0; // seconds
    uint32_t packetSize = 512; // bytes
    Time interPacketInterval = MilliSeconds (500); // milliseconds

    // Command line arguments for flexible simulation
    CommandLine cmd (__FILE__);
    cmd.AddValue ("simTime", "Total simulation time (s)", simTime);
    cmd.AddValue ("packetSize", "UDP packet size (bytes)", packetSize);
    cmd.AddValue ("interPacketInterval", "UDP client inter-packet interval (ms)", interPacketInterval);
    cmd.Parse (argc, argv);

    // 1. Create Nodes: one gNB and one UE
    NodeContainer gnbNodes;
    gnbNodes.Create (1);
    NodeContainer ueNodes;
    ueNodes.Create (1);

    // 2. EPC Setup: Create an EPC helper (which includes MME, SGW, PGW functionalities)
    Ptr<NrEpcHelper> epcHelper = CreateObject<NrEpcHelper> ();

    // 3. NR Helper Setup: Configure NR parameters and associate with EPC
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper> ();
    nrHelper->SetEpcHelper (epcHelper);

    // Configure NR Channel and Pathloss Model
    // Using 3GPP channel model and urban macro campus propagation loss model
    nrHelper->SetAttribute ("ChannelModelType", StringValue ("ns3::ThreeGppChannelModel"));
    nrHelper->SetAttribute ("PathlossModelType", StringValue ("ns3::ThreeGppUrbanMacroCampusPropagationLossModel"));
    nrHelper->SetAttribute ("Bandwidth", DoubleValue (100e6)); // 100 MHz bandwidth
    nrHelper->SetAttribute ("Frequency", DoubleValue (3.5e9)); // 3.5 GHz frequency

    // 4. Install Internet Stack on all nodes
    InternetStackHelper internet;
    internet.Install (gnbNodes);
    internet.Install (ueNodes);

    // 5. Install NR NetDevices on gNB and UE, then attach UE to gNB
    NetDeviceContainer gnbDevs = nrHelper->InstallGnbDevice (gnbNodes.Get (0));
    NetDeviceContainer ueDevs = nrHelper->InstallUeDevice (ueNodes.Get (0));

    // Attach UE to the gNB (this initiates control plane setup via EPC)
    nrHelper->AttachToNewGnb (ueDevs.Get (0), gnbDevs.Get (0));

    // 6. Assign IP addresses for UE (gNB's IP for application server needs to be retrieved)
    // The EPC helper assigns IP addresses to UEs from its internal pool (e.g., 10.x.x.x)
    Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueDevs.Get (0)));
    Ipv4Address ueIp = ueIpIfaces.GetAddress (0); // UE's assigned IP address

    // Get gNB's IP address for the UDP server application.
    // The gNB's user plane interface (S1-U) will have an IP address assigned by the EPC.
    // We iterate through gNB's IPv4 interfaces to find the first non-loopback address.
    Ptr<Ipv4> gnbIpv4 = gnbNodes.Get (0)->GetObject<Ipv4> ();
    Ipv4Address gnbAppAddress;
    for (uint32_t i = 0; i < gnbIpv4->GetNInterfaces (); ++i)
    {
        Ipv4InterfaceAddress addr = gnbIpv4->GetAddress (i, 0);
        if (addr.GetLocal () != Ipv4Address::GetLoopback ())
        {
            gnbAppAddress = addr.GetLocal ();
            break;
        }
    }
    NS_ASSERT (!gnbAppAddress.IsZero ()); // Ensure a valid IP address was found for gNB
    NS_LOG_INFO ("gNB application server IP address: " << gnbAppAddress);


    // 7. Mobility Setup
    MobilityHelper mobility;

    // gNB: Stationary at the origin (0,0,0)
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (gnbNodes);
    gnbNodes.Get (0)->GetObject<ConstantPositionMobilityModel> ()->SetPosition (Vector (0.0, 0.0, 0.0));

    // UE: Moving with a constant velocity
    mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
    mobility.Install (ueNodes);
    Ptr<ConstantVelocityMobilityModel> ueMobility = ueNodes.Get (0)->GetObject<ConstantVelocityMobilityModel> ();
    ueMobility->SetPosition (Vector (0.0, 50.0, 0.0)); // Start 50 meters away from gNB on Y-axis
    ueMobility->SetVelocity (Vector (10.0, 0.0, 0.0)); // Move at 10 m/s along X-axis

    // 8. Applications: UDP client on UE, UDP server on gNB
    uint16_t port = 9; // Standard discard port

    // Server on gNB node
    UdpServerHelper serverHelper (port);
    ApplicationContainer serverApps = serverHelper.Install (gnbNodes.Get (0));
    serverApps.Start (Seconds (0.0)); // Server starts at the beginning of simulation
    serverApps.Stop (Seconds (simTime)); // Server runs until simulation end

    // Client on UE node
    // It targets the gNB's IP address (gnbAppAddress)
    UdpClientHelper clientHelper (gnbAppAddress, port);
    clientHelper.SetAttribute ("MaxPackets", UintegerValue (1000000)); // Send a large number of packets
    clientHelper.SetAttribute ("Interval", TimeValue (interPacketInterval));
    clientHelper.SetAttribute ("PacketSize", UintegerValue (packetSize));
    ApplicationContainer clientApps = clientHelper.Install (ueNodes.Get (0));
    clientApps.Start (Seconds (1.0)); // Client starts after 1 second
    clientApps.Stop (Seconds (simTime - 1.0)); // Client stops 1 second before simulation end

    // 9. Pcap Tracing for network devices
    // This will generate .pcap files to analyze traffic.
    nrHelper->EnablePcap ("nr-one-gnb-one-ue-mobility", gnbDevs, true);
    nrHelper->EnablePcap ("nr-one-gnb-one-ue-mobility", ueDevs, true);

    // 10. Run Simulation
    Simulator::Stop (Seconds (simTime));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}