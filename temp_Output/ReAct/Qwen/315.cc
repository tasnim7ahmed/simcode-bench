#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/nr-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    uint32_t numUe = 1;
    uint32_t numGnb = 1;
    double simTime = 10.0;
    double udpPacketSize = 1024;
    uint32_t packetsToSend = 1000;
    double interPacketInterval = 0.01;

    Config::SetDefault("ns3::NrSpectrumPhy::CtrlErrorModelEnabled", BooleanValue(false));
    Config::SetDefault("ns3::NrSpectrumPhy::DataErrorModelEnabled", BooleanValue(false));

    NodeContainer ueNodes;
    NodeContainer gnbNodes;
    ueNodes.Create(numUe);
    gnbNodes.Create(numGnb);

    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // UE
    positionAlloc->Add(Vector(10.0, 0.0, 0.0)); // gNB
    MobilityHelper mobility;
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(ueNodes);
    mobility.Install(gnbNodes);

    NrPointToPointEpcHelper epcHelper;
    epcHelper.SetS1uLinkDelay(Time::FromSeconds(0.010));

    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();

    nrHelper->SetChannelConditionModelAttribute("UpdatePeriod", TimeValue(MilliSeconds(100)));
    nrHelper->SetPathlossAttribute("ShadowingEnabled", BooleanValue(false));

    NetDeviceContainer ueDevs;
    NetDeviceContainer gnbDevs;

    gnbDevs = nrHelper->InstallGnbDevice(gnbNodes, epcHelper.GetGnbBearer());
    ueDevs = nrHelper->InstallUeDevice(ueNodes, epcHelper.GetUeBearer());

    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(gnbNodes);

    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper.AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
        Ptr<Node> ueNode = ueNodes.Get(i);
        Ipv4StaticRoutingHelper ipv4RoutingHelper;
        Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ueNode->GetObject<Ipv4>());
        NS_ASSERT(ueStaticRouting);
        ueStaticRouting->SetDefaultRoute(epcHelper.GetUeDefaultGatewayAddress(), 1);
    }

    ApplicationContainer serverApps;
    UdpServerHelper server(9);
    serverApps.Add(server.Install(ueNodes.Get(0)));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    ApplicationContainer clientApps;
    UdpClientHelper client(ueIpIface.GetAddress(0), 9);
    client.SetAttribute("MaxPackets", UintegerValue(packetsToSend));
    client.SetAttribute("Interval", TimeValue(Seconds(interPacketInterval)));
    client.SetAttribute("PacketSize", UintegerValue(udpPacketSize));
    clientApps.Add(client.Install(gnbNodes.Get(0)));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}