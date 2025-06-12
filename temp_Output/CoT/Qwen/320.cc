#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    double simDuration = 10.0;
    uint32_t packetCount = 1000;
    Time interPacketInterval = Seconds(0.01);
    uint16_t port = 9;
    uint32_t payloadSize = 1024;

    Config::SetDefault("ns3::LteHelper::UseRlcSm", BooleanValue(true));
    Config::SetDefault("ns3::UdpClient::MaxPackets", UintegerValue(packetCount));
    Config::SetDefault("ns3::UdpClient::Interval", TimeValue(interPacketInterval));
    Config::SetDefault("ns3::UdpClient::PacketSize", UintegerValue(payloadSize));

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);
    lteHelper->SetAttribute("PathlossModel", StringValue("ns3::FriisSpectrumPropagationLossModel"));

    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(3);

    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    Ipv4AddressHelper ipv4H;
    ipv4H.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer enbIfs = epcHelper->AssignUeIpv4Address(ueDevs);
    Ipv4InterfaceContainer ueIfs;
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
        ueIfs.Add(ueNodes.Get(i)->GetObject<Ipv4>()->GetInterface(1));
    }

    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(enbNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simDuration));

    UdpClientHelper clientHelper(enbIfs.GetAddress(0), port);
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
        clientApps.Add(clientHelper.Install(ueNodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simDuration));

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    mobility.Install(ueNodes);

    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}