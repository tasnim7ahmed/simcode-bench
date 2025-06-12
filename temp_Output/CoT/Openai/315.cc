#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/point-to-point-epc-helper.h"
#include "ns3/antenna-module.h"
#include "ns3/config-store.h"
#include "ns3/nr-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    nrHelper->SetEpcHelper(epcHelper);
    nrHelper->SetGnbDeviceAttribute("AntennaNum", UintegerValue(2));
    nrHelper->SetUeDeviceAttribute("AntennaNum", UintegerValue(2));

    NodeContainer ueNodes;
    ueNodes.Create(1);

    NodeContainer gnbNodes;
    gnbNodes.Create(1);

    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);

    Ptr<Node> pgw = epcHelper->GetPgwNode();

    // Create Internet stack on remote host
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    // Create Point-to-Point link between remoteHost and PGW
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gbps")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHostContainer.Get(0));

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);

    // remoteHost as default gateway
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);

    // Install mobility model
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gnbNodes);
    mobility.Install(ueNodes);

    // Configure NR channel and scenario
    nrHelper->SetSchedulerTypeId(TypeId::LookupByName("ns3::NrMacSchedulerTdmaQos"));
    nrHelper->SetPathlossAttribute("ShadowingEnabled", BooleanValue(false));
    nrHelper->InitializeOperationBand(28e9, 100e6); // FR2 28GHz, 100MHz bw

    // Install gNB and UE devices
    NetDeviceContainer gnbNetDev = nrHelper->InstallGnbDevice(gnbNodes, 1, 1);
    NetDeviceContainer ueNetDev = nrHelper->InstallUeDevice(ueNodes);

    // Attach UE to the gNB
    nrHelper->AttachToClosestEnb(ueNetDev, gnbNetDev);

    // Activate default EPS bearer
    enum EpsBearer::Qci q = EpsBearer::NGBR_VOICE_VIDEO_GAMING;
    EpsBearer bearer(q);
    nrHelper->ActivateDataRadioBearer(ueNetDev, bearer);

    // Install internet stack on the UE nodes
    internet.Install(ueNodes);

    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueNetDev));

    // Set default route for UEs via EPC
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
    {
        Ptr<Node> ueNode = ueNodes.Get(u);
        Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> (ueNode->GetObject<Ipv4>()->GetRoutingProtocol());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // UDP Server on UE port 9
    uint16_t port = 9;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApps = udpServer.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP Client on gNB, targeting the UE's IP on port 9
    UdpClientHelper udpClient(ueIpIfaces.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = udpClient.Install(gnbNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable Logging
    // Uncomment for debugging:
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}