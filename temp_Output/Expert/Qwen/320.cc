#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteUdpSimulation");

int main(int argc, char *argv[]) {
    uint16_t numEnb = 1;
    uint16_t numUe = 3;
    double simTime = 10.0;
    double serverStart = 1.0;
    double clientStart = 2.0;
    uint32_t packetSize = 1024;
    uint32_t packetCount = 1000;
    double interval = 0.01;

    Config::SetDefault("ns3::LteHelper::UseIdealRrc", BooleanValue(true));
    Config::SetDefault("ns3::LteHelper::UseIdealRlc", BooleanValue(true));

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);
    lteHelper->SetAttribute("PathlossModel", StringValue("ns3::FriisSpectrumPropagationLossModel"));

    NodeContainer enbNodes;
    enbNodes.Create(numEnb);
    NodeContainer ueNodes;
    ueNodes.Create(numUe);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    mobility.Install(ueNodes);

    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));
    Ipv4Address enbAddr = epcHelper->GetSgwPgwNode()->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();

    for (uint32_t u = 0; u < ueNodes.GetN(); ++u) {
        lteHelper->Attach(ueDevs.Get(u), enbDevs.Get(0));
    }

    UdpServerHelper server(9);
    ApplicationContainer serverApps = server.Install(enbNodes.Get(0));
    serverApps.Start(Seconds(serverStart));
    serverApps.Stop(Seconds(simTime));

    UdpClientHelper client(enbAddr, 9);
    client.SetAttribute("MaxPackets", UintegerValue(packetCount));
    client.SetAttribute("Interval", TimeValue(Seconds(interval)));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = client.Install(ueNodes);
    clientApps.Start(Seconds(clientStart));
    clientApps.Stop(Seconds(simTime));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}