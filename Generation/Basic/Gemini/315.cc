#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/nr-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"

int main(int argc, char *argv[])
{
    // Simulation parameters from the description
    double simTime = 10.0; // seconds
    uint32_t packetCount = 1000;
    uint32_t packetSize = 1024; // bytes
    double packetInterval = 0.01; // seconds
    uint16_t udpPort = 9;
    double serverStartTime = 1.0; // seconds
    double clientStartTime = 2.0; // seconds

    // Create nodes for UE and gNB
    ns3::NodeContainer ueNodes;
    ueNodes.Create(1);
    ns3::NodeContainer gnbNodes;
    gnbNodes.Create(1);

    // Set up mobility for UE and gNB
    // gNB at origin (0,0,0)
    ns3::Ptr<ns3::MobilityModel> gnbMobility = ns3::CreateObject<ns3::ConstantPositionMobilityModel>();
    gnbMobility->SetPosition(ns3::Vector(0.0, 0.0, 0.0));
    gnbNodes.Get(0)->SetMobility(gnbMobility);

    // UE 10 meters away from gNB (10,0,0)
    ns3::Ptr<ns3::MobilityModel> ueMobility = ns3::CreateObject<ns3::ConstantPositionMobilityModel>();
    ueMobility->SetPosition(ns3::Vector(10.0, 0.0, 0.0));
    ueNodes.Get(0)->SetMobility(ueMobility);

    // Set up 5G NR network helper
    ns3::Ptr<ns3::NrHelper> nrHelper = ns3::CreateObject<ns3::NrHelper>();
    nrHelper->Set5gCoreType(ns3::NrHelper::SA_5GC_ARCHITECTURE); // Use Standalone 5G Core

    // Create the 5G Core Network (AMF, UPF, SMF nodes are created internally)
    nrHelper->Create5gCore();

    // Install Internet stack on gNB and UE nodes
    nrHelper->InstallInternetStack(gnbNodes);
    nrHelper->InstallInternetStack(ueNodes);

    // Create NR devices for the UE and gNB and establish the link
    // This call handles the attachment of the gNB to the 5GC and the UE to the gNB.
    ns3::NetDeviceContainer gnbDevs;
    ns3::NetDeviceContainer ueDevs;
    nrHelper->CreateNrDevice(ueNodes.Get(0), gnbNodes.Get(0), ueDevs, gnbDevs);

    // Get the IP address assigned to the UE for the client to target
    ns3::Ipv4InterfaceContainer ueIpv4If = nrHelper->GetUeIpv4Interfaces();
    ns3::Ipv4Address ueIpAddress = ueIpv4If.GetAddress(0);

    // Install UDP Server on the UE
    ns3::UdpEchoServerHelper echoServer(udpPort);
    ns3::ApplicationContainer serverApps = echoServer.Install(ueNodes.Get(0));
    serverApps.Start(ns3::Seconds(serverStartTime));
    serverApps.Stop(ns3::Seconds(simTime)); // Server runs until simulation end

    // Install UDP Client on the gNB
    ns3::UdpEchoClientHelper echoClient(ueIpAddress, udpPort);
    echoClient.SetAttribute("MaxPackets", ns3::UintegerValue(packetCount));
    echoClient.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(packetInterval)));
    echoClient.SetAttribute("PacketSize", ns3::UintegerValue(packetSize));
    ns3::ApplicationContainer clientApps = echoClient.Install(gnbNodes.Get(0));
    clientApps.Start(ns3::Seconds(clientStartTime));
    clientApps.Stop(ns3::Seconds(simTime)); // Client attempts to send packets until simulation end

    // Set simulation stop time
    ns3::Simulator::Stop(ns3::Seconds(simTime));

    // Run the simulation
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}