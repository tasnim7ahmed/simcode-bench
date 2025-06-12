#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes: one eNodeB and one UE
    NodeContainer enbNodes;
    NodeContainer ueNodes;
    enbNodes.Create(1);
    ueNodes.Create(1);

    // Create the LTE helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

    // Install LTE devices
    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install internet stack on nodes
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    // Assign IP addresses to UEs
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = lteHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Attach UEs to the first eNodeB
    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    // Set TCP client-server application
    uint16_t dlPort = 12345;

    // Server (on eNodeB side)
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), dlPort));
    PacketSinkHelper dlPacketSinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer serverApps = dlPacketSinkHelper.Install(enbNodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // Client (on UE side)
    InetSocketAddress rmtAddr(ueIpIface.GetAddress(0), dlPort);
    BulkSendHelper dlBulkSendHelper("ns3::TcpSocketFactory", rmtAddr);
    dlBulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0)); // Send continuously
    ApplicationContainer clientApps = dlBulkSendHelper.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    // Set up mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    mobility.Install(ueNodes);

    // Enable logging
    lteHelper->EnableTraces();

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}