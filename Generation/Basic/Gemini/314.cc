#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"

int main(int argc, char *argv[])
{
    // Enable logging for UDP applications to see packet reception
    ns3::LogComponentEnable("UdpServer", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("UdpClient", ns3::LOG_LEVEL_INFO);

    // Set time resolution to nanoseconds (default is usually fine, but explicit)
    ns3::Time::SetResolution(ns3::Time::NS);

    // Create an LTE helper and EPC helper
    ns3::Ptr<ns3::LteHelper> lteHelper = ns3::CreateObject<ns3::LteHelper>();
    ns3::Ptr<ns3::EpcHelper> epcHelper = ns3::CreateObject<ns3::EpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Configure the PGW/SGW node (part of EPC helper)
    // ns3::Ptr<ns3::Node> pgw = epcHelper->GetPgwNode(); // Not directly used but good to know

    // Create nodes for eNodeB and UE
    ns3::NodeContainer enbNodes;
    enbNodes.Create(1);
    ns3::NodeContainer ueNodes;
    ueNodes.Create(1);

    // Install Mobility on eNodeB and UE
    ns3::MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    mobility.Install(ueNodes);

    // Install LTE devices on eNodeB and UE nodes
    ns3::NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    ns3::NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack on the eNodeB node
    // The UE gets its IP stack installed by the EpcHelper during the Attach process
    ns3::InternetStackHelper internet;
    internet.Install(enbNodes);

    // Attach UE to eNodeB
    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

    // Get the UE's assigned IP address from the EPC helper
    // This IP address is crucial for the client to know where to send packets.
    ns3::Ipv4Address ueIpAddress = epcHelper->GetUeIpv4Address(ueLteDevs.Get(0));

    // Setup UDP Server on UE
    uint16_t serverPort = 9; // UDP server port
    ns3::UdpServerHelper server(serverPort);
    ns3::ApplicationContainer serverApps = server.Install(ueNodes.Get(0));
    serverApps.Start(ns3::Seconds(1.0));
    serverApps.Stop(ns3::Seconds(10.0)); // Server stops at 10s, as simulation ends at 10s

    // Setup UDP Client on eNodeB
    uint32_t packetSize = 1024;
    uint32_t numPackets = 1000;
    ns3::Time interPacketInterval = ns3::Seconds(0.01); // 10ms interval

    ns3::UdpClientHelper client(ueIpAddress, serverPort);
    client.SetAttribute("MaxPackets", ns3::UintegerValue(numPackets));
    client.SetAttribute("Interval", ns3::TimeValue(interPacketInterval));
    client.SetAttribute("PacketSize", ns3::UintegerValue(packetSize));
    ns3::ApplicationContainer clientApps = client.Install(enbNodes.Get(0));
    clientApps.Start(ns3::Seconds(2.0));
    clientApps.Stop(ns3::Seconds(10.0)); // Client stops at 10s, as simulation ends at 10s

    // Set simulation duration
    ns3::Simulator::Stop(ns3::Seconds(10.0));

    // Run the simulation
    ns3::Simulator::Run();

    // Cleanup simulation resources
    ns3::Simulator::Destroy();

    return 0;
}