#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Set up logging
    LogComponentEnable("LteHelper", LOG_LEVEL_INFO);
    LogComponentEnable("PointToPointHelper", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);

    // 1. LTE Network Setup
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointHelper> p2pHelper = CreateObject<PointToPointHelper>(); // For EPC's internal links
    Ptr<EpcHelper> epcHelper = CreateObject<EpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Create nodes for eNodeB and UEs
    NodeContainer enbNodes;
    enbNodes.Create(1); // One eNodeB

    NodeContainer ueNodes;
    ueNodes.Create(3); // Three UEs

    // Install Internet Stack on all nodes
    InternetStackHelper internet;
    internet.Install(enbNodes);
    internet.Install(ueNodes);

    // Set Mobility (static position for simplicity)
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    mobility.Install(ueNodes);

    // Install LTE devices
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Assign IP addresses to UEs (done by EpcHelper automatically)
    epcHelper->AssignUeIpv4Address(ueLteDevs);

    // Attach UEs to the eNodeB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        lteHelper->Attach(ueLteDevs.Get(i), enbLteDevs.Get(0));
    }

    // Get the IP address of the eNodeB's S1 interface for the server
    // The EpcHelper assigns this IP when setting up the S1 link.
    // It is typically the first IP address assigned to the eNB's S1 interface.
    Ipv4InterfaceContainer enbIpIfaces = epcHelper->AssignEnbIpv4Address(enbLteDevs);
    Ipv4Address enbS1Ip = enbIpIfaces.GetAddress(0);

    // 2. UDP Server Setup on eNodeB
    uint16_t serverPort = 9;
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory",
                                      InetSocketAddress(Ipv4Address::GetAny(), serverPort));
    ApplicationContainer serverApps = packetSinkHelper.Install(enbNodes.Get(0));
    serverApps.Start(Seconds(1.0)); // Server starts at 1 second
    serverApps.Stop(Seconds(10.0)); // Server runs throughout the simulation

    // 3. UDP Clients Setup on UEs
    uint32_t clientPacketCount = 1000;
    uint32_t clientPacketSize = 1024; // 1024 bytes
    Time clientInterval = MilliSeconds(10); // 0.01 seconds

    UdpClientHelper udpClientHelper(enbS1Ip, serverPort);
    udpClientHelper.SetAttribute("MaxPackets", UintegerValue(clientPacketCount));
    udpClientHelper.SetAttribute("Interval", TimeValue(clientInterval));
    udpClientHelper.SetAttribute("PacketSize", UintegerValue(clientPacketSize));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        clientApps.Add(udpClientHelper.Install(ueNodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0)); // Clients start at 2 seconds
    clientApps.Stop(Seconds(10.0)); // Clients run until simulation stops at 10 seconds

    // Set simulation duration
    Simulator::Stop(Seconds(10.0));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}