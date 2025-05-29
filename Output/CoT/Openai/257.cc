#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-epc-helper.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create two UEs and one eNodeB
    NodeContainer ueNodes;
    ueNodes.Create(2);
    NodeContainer enbNodes;
    enbNodes.Create(1);

    // EPC and LTE helpers
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Mobility: place nodes in a grid
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(40.0),
                                 "DeltaY", DoubleValue(40.0),
                                 "GridWidth", UintegerValue(2),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    mobility.Install(ueNodes);

    // Install LTE devices to the nodes
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the Internet stack on UEs
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IP addresses via EPC
    Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));
    // Attach UEs to eNodeB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        lteHelper->Attach(ueLteDevs.Get(i), enbLteDevs.Get(0));
    }

    // Allow remote IP traffic (bearer activation)
    lteHelper->ActivateDedicatedEpsBearer(ueLteDevs, EpsBearer(EpsBearer::NGBR_VIDEO_TCP_DEFAULT), EpcTft::Default());

    // Install applications: UDP Server on UE 1 (index 1)
    uint16_t serverPort = 9;
    UdpServerHelper udpServer(serverPort);
    ApplicationContainer serverApps = udpServer.Install(ueNodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Client on UE 0 (index 0), sending 2 packets of 512 bytes to UE 1
    UdpClientHelper udpClient(ueIpIfaces.GetAddress(1), serverPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(2));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    udpClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = udpClient.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable packet capture
    lteHelper->EnableTraces();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}