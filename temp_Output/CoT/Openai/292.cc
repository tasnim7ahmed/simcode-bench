#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ngc-helper.h"
#include "ns3/nr-helper.h"
#include "ns3/nr-point-to-point-epc-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Simulation parameters
    double simTime = 10.0;
    uint32_t packetSize = 512;
    double interval = 0.5; // 500 ms

    // Create nodes: 1 gNB + 1 UE
    NodeContainer ueNodes;
    ueNodes.Create(1);
    NodeContainer gNbNodes;
    gNbNodes.Create(1);

    // NR + EPC
    Ptr<NrPointToPointEpcHelper> epcHelper = CreateObject<NrPointToPointEpcHelper>();
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
    nrHelper->SetEpcHelper(epcHelper);

    // Spectrum configuration
    nrHelper->SetPathlossAttribute("Frequency", DoubleValue(28e9));
    nrHelper->SetSchedulerTypeId(TypeId::LookupByName("ns3::NrMacSchedulerTdmaRR"));

    // Install NR devices to UE and gNB
    NetDeviceContainer gNbDevs = nrHelper->InstallGnbDevice(gNbNodes);
    NetDeviceContainer ueDevs = nrHelper->InstallUeDevice(ueNodes);

    // Core network: PGW
    Ptr<Node> pgw = epcHelper->GetPgwNode();

    // Attach UE to gNB
    nrHelper->AttachToClosestEnb(ueDevs, gNbDevs);

    // Mobility
    // gNB stationary
    Ptr<ListPositionAllocator> gNbPosAlloc = CreateObject<ListPositionAllocator>();
    gNbPosAlloc->Add(Vector(0.0, 0.0, 0.0));
    MobilityHelper gNbMobility;
    gNbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    gNbMobility.SetPositionAllocator(gNbPosAlloc);
    gNbMobility.Install(gNbNodes);

    // UE mobility: moves on Y axis
    MobilityHelper ueMobility;
    ueMobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    ueMobility.Install(ueNodes);
    ueNodes.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetPosition(Vector(10.0, 0.0, 1.5));
    ueNodes.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(0.0, 2.0, 0.0)); // 2 m/s along Y

    // Install Internet stack to UE and gNB
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IP address to UEs from EPC
    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Set default gateway for UE
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(ueNodes.Get(i)->GetObject<Ipv4>()->GetRoutingProtocol());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // Application: UDP server on gNB side (on remote host connected to PGW)
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);

    // Connect remoteHost to PGW
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);

    // Assign IP address to remoteHost
    InternetStackHelper internet_remote;
    internet_remote.Install(remoteHost);
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer remoteHostIfaces = ipv4h.Assign(internetDevices);
    Ipv4Address remoteHostAddr = remoteHostIfaces.GetAddress(1);

    // Configure static routing for remote host
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(remoteHost->GetObject<Ipv4>()->GetRoutingProtocol());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // UDP server on remote host
    uint16_t udpPort = 8000;
    UdpServerHelper udpServer(udpPort);
    ApplicationContainer serverApps = udpServer.Install(remoteHost);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime));

    // UDP client on UE, sends to remote host
    UdpClientHelper udpClient(remoteHostAddr, udpPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
    udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApps = udpClient.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simTime));

    // Enable pcap tracing (optional)
    p2ph.EnablePcapAll("nr-epc-5g-single");

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}