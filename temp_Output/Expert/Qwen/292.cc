#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-module.h"
#include "ns3/epc-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Nr5GEpcSimulation");

int main(int argc, char *argv[])
{
    double simTime = 10.0;
    uint32_t packetSize = 512;
    Time interPacketInterval = MilliSeconds(500);

    Config::SetDefault("ns3::UdpClient::MaxPackets", UintegerValue(0xFFFFFFFF));
    Config::SetDefault("ns3::UdpClient::Interval", TimeValue(interPacketInterval));
    Config::SetDefault("ns3::UdpClient::PacketSize", UintegerValue(packetSize));

    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
    Ptr<EpcHelper> epcHelper = EpcHelper::GetEpcHelper("ns3::PointToPointEpcHelper");

    NodeContainer gNbNodes;
    gNbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(1);

    nrHelper->SetGnbDeviceAttribute("Numerology", UintegerValue(1));
    nrHelper->SetGnbDeviceAttribute("Earfcn", UintegerValue(343260));
    nrHelper->SetUeDeviceAttribute("Earfcn", UintegerValue(343260));

    NetDeviceContainer gNbDevs = nrHelper->InstallGnbDevice(gNbNodes, epcHelper->GetS1uGtpuSocketFactory());
    NetDeviceContainer ueDevs = nrHelper->InstallUeDevice(ueNodes, epcHelper->GetS1uGtpuSocketFactory());

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gNbNodes);

    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(ueNodes);
    ueNodes.Get(0)->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(10, 0, 0));

    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(gNbNodes);

    Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));
    Ipv4Address gNbAddr = epcHelper->GetGnbIpv4Address();

    for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
    {
        Ptr<Node> ueNode = ueNodes.Get(u);
        Ptr<Ipv4StaticRouting> ipv4StaticRouting = Ipv4RoutingHelper::GetStaticRouting(ueNode->GetObject<Ipv4>());
        ipv4StaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    ApplicationContainer serverApps;
    ApplicationContainer clientApps;

    UdpServerHelper server(8000);
    serverApps.Add(server.Install(gNbNodes.Get(0)));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime));

    UdpClientHelper client(ueIpIfaces.GetAddress(0), 8000);
    client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    client.SetAttribute("Interval", TimeValue(interPacketInterval));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));
    clientApps.Add(client.Install(ueNodes.Get(0)));
    clientApps.Start(Seconds(0.1));
    clientApps.Stop(Seconds(simTime));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}